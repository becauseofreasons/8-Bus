#!/usr/bin/env python3
"""Reference model of the 8-bus channel EQ (from the schematic of the channel board).

Pure Python AC analysis (modified nodal analysis, ideal op-amps). This file is the reference for the
filter curves: the plugin builds the same circuits in C++ (src/core/circuits.cpp) and test/curves.cpp
compares both. Run:  python3 circuit_ref.py [--table out.txt]

Pots: rotation 0..1 (0 = fully counter clockwise). Freq pots are rheostats with a log taper
(15 % at mid travel), boost/cut pots are linear, wiper toward the boosting end at 1.
"""
import cmath, math, sys

# ---------------------------------------------------------------- tapers (keep in step with C++)
def taper_log(x, mid=0.15):
    """audio taper: resistance fraction at travel x (0..1), mid = fraction at x = 0.5"""
    r = (1.0 / mid - 1.0) ** 2           # a with (sqrt(a)+1) = 1/mid
    return (r ** x - 1.0) / (r - 1.0)

def rheo_freq(rot, rtot, mid=0.15):
    """frequency rheostat: clockwise = less resistance = higher frequency"""
    return rtot * taper_log(1.0 - rot, mid)

RMIN = 1.0  # ohm, avoids zero-ohm pot halves

# ---------------------------------------------------------------- tiny MNA solver
class Circuit:
    def __init__(self):
        self.nodes = {'gnd': -1}
        self.R, self.C, self.OA = [], [], []
        self.vin = None
    def n(self, name):
        if name not in self.nodes:
            self.nodes[name] = len(self.nodes) - 1
        return self.nodes[name]
    def r(self, a, b, val): self.R.append((self.n(a), self.n(b), max(val, RMIN)))
    def c(self, a, b, val): self.C.append((self.n(a), self.n(b), val))
    def opamp(self, plus, minus, out): self.OA.append((self.n(plus), self.n(minus), self.n(out)))
    def source(self, node): self.vin = self.n(node)

    def solve(self, f, outs):
        s = 2j * math.pi * f
        N = len(self.nodes) - 1
        M = N + len(self.OA) + 1
        A = [[0j] * M for _ in range(M)]
        z = [0j] * M
        def stamp(a, b, y):
            if a >= 0: A[a][a] += y
            if b >= 0: A[b][b] += y
            if a >= 0 and b >= 0: A[a][b] -= y; A[b][a] -= y
        for a, b, v in self.R: stamp(a, b, 1.0 / v)
        for a, b, v in self.C: stamp(a, b, s * v)
        for k, (p, m, o) in enumerate(self.OA):
            col = N + k
            A[o][col] += 1.0                 # output current into the output node
            if p >= 0: A[col][p] += 1.0      # constraint v+ - v- = 0
            if m >= 0: A[col][m] -= 1.0
        col = N + len(self.OA)
        A[self.vin][col] += 1.0
        A[col][self.vin] += 1.0
        z[col] = 1.0
        x = gauss(A, z)
        return [x[self.nodes[o]] for o in outs]

def gauss(A, z):
    n = len(z)
    A = [row[:] + [z[i]] for i, row in enumerate(A)]
    for i in range(n):
        p = max(range(i, n), key=lambda r: abs(A[r][i]))
        A[i], A[p] = A[p], A[i]
        piv = A[i][i]
        if abs(piv) < 1e-300: raise ZeroDivisionError("singular")
        for r in range(i + 1, n):
            f = A[r][i] / piv
            if f != 0:
                for c in range(i, n + 1): A[r][c] -= f * A[i][c]
    x = [0j] * n
    for i in range(n - 1, -1, -1):
        x[i] = (A[i][n] - sum(A[i][c] * x[c] for c in range(i + 1, n))) / A[i][i]
    return x

# ---------------------------------------------------------------- the sections
def locut():
    c = Circuit(); c.source('in')
    c.c('in', 'a', 0.22e-6); c.r('a', 'gnd', 5.1e3)
    c.c('a', 'b', 0.22e-6);  c.r('b', 'out', 1.3e3)
    c.c('b', 'p', 0.22e-6);  c.r('p', 'gnd', 62e3)
    c.opamp('p', 'out', 'out')           # U1A follower
    return c, 'out'

def mid1(lm_gain, lm_freq):
    """U2B with the lo-mid Wien bridge. lm_gain 0..1 (0.5 flat), lm_freq 0..1"""
    c = Circuit(); c.source('in')
    c.r('in', 'nm', 10e3); c.r('nm', 'A', 10e3)              # R29, R30
    c.opamp('P', 'nm', 'A')
    c.r('in', 'w', lm_gain * 10e3); c.r('w', 'A', (1 - lm_gain) * 10e3)    # R112 10KB
    rf = rheo_freq(lm_freq, 500e3)                            # R111A/B 500KC
    c.c('w', 's1', 0.01e-6); c.r('s1', 's2', 8.2e3); c.r('s2', 'P', rf)   # C110 R109 R111A
    c.r('P', 's3', rf); c.r('s3', 'gnd', 8.2e3); c.c('P', 'gnd', 3.9e-9)  # R111B R110 C111
    return c, 'A'

R20 = 3.6e3   # 3K6 on the module schematic (3K3 on the channel board drawing); no influence (op-amp input)

def mid2(hm_gain, hm_freq, hm_q):
    """U3A with the hi-mid state variable filter. hm_q 0..1 (0 = wide)"""
    c = Circuit(); c.source('in')
    c.r('in', 'm', 10e3); c.r('m', 'B', 10e3)               # R25, R24
    c.opamp('q', 'm', 'B'); c.r('BP', 'q', R20)
    # the band pass output is inverted: the wiper toward the input end (A) boosts
    c.r('in', 'w', (1 - hm_gain) * 50e3); c.r('w', 'B', hm_gain * 50e3)  # R113 50KB
    c.c('w', 't', 0.018e-6); c.r('t', 'N1', 11e3)             # C139, R16
    rq = 50e3
    c.r('N1', 'P7', 160.0); c.r('P7', 'gnd', q_lower(hm_q) * rq)            # R28, R115 lower part
    c.r('P8', 'gnd', (1 - q_lower(hm_q)) * rq); c.r('P8', 'gnd', 2.7e3)    # R115 upper part, R34
    c.r('P8', 'N2', 2.7e3)                                     # R31
    c.r('BP', 'N1', 27e3)                                      # R19
    c.opamp('N1', 'N2', 'HP'); c.r('N2', 'HP', 27e3); c.r('N2', 'LP', 27e3)  # U4B, R15, R21
    rf = rheo_freq(hm_freq, 50e3)                              # R164A/B 50KC
    c.r('HP', 'r1', rf); c.r('r1', 'n3', 1.3e3); c.c('n3', 'BP', 6.8e-9); c.opamp('gnd', 'n3', 'BP')  # U3B
    c.r('BP', 'r2', rf); c.r('r2', 'n4', 1.3e3); c.c('n4', 'LP', 6.8e-9); c.opamp('gnd', 'n4', 'LP')  # U4A
    return c, 'B'

def q_of_lower(rlow):
    """Q of the state variable filter for the lower part of R115 (ideal analysis, see Analyse.md)"""
    rup = 50e3 - rlow
    k = (1 / 27e3) / (1 / 11e3 + 1 / 27e3 + 1 / (160.0 + rlow))      # band pass share at N1
    rn2 = 2.7e3 + 1 / (1 / 2.7e3 + 1 / max(rup, RMIN))                # N2 to ground
    g = 2 + 27e3 / rn2                                                 # gain of U4B from N1
    return 1 / (k * g)

def bw_of_q(q): return 2 * math.asinh(1 / (2 * q)) / math.log(2)

def bw_target(rot):
    """BW knob: 3 octaves (left) - 2 octaves (middle, 'normal') - 1/12 octave (right), log spaced"""
    l0, l1, l2 = math.log2(3), 1.0, math.log2(1 / 12)
    l = l0 + (l1 - l0) * rot / 0.5 if rot < 0.5 else l1 + (l2 - l1) * (rot - 0.5) / 0.5
    return 2 ** l

def q_lower(rot):
    """BW pot R115 50KG, wiper to ground: share of the pot between P7 and the wiper.
    The G taper is not documented; it is chosen so that the knob follows bw_target (keep in step with C++)."""
    target = bw_target(rot)
    lo, hi = 0.0, 50e3                       # more resistance = wider
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if bw_of_q(q_of_lower(mid)) < target: lo = mid
        else: hi = mid
    return 0.5 * (lo + hi) / 50e3

def hilo(hi, lo):
    """Baxandall around U1B. hi/lo 0..1 (0.5 flat, 1 boost). Output inverted (U2A follows)."""
    c = Circuit(); c.source('in')
    c.c('in', 'h1', 6.8e-9); c.r('h1', 'hw', (1 - hi) * 50e3); c.r('hw', 'h2', hi * 50e3)
    c.c('h2', 'Y', 6.8e-9); c.r('hw', 'X', 3.9e3)                       # C105 R106 C106 R7
    c.r('in', 'l1', 5.6e3); c.r('l1', 'lw', (1 - lo) * 50e3); c.r('lw', 'l2', lo * 50e3)
    c.r('l2', 'Y', 5.6e3); c.c('l1', 'lw', 0.033e-6); c.c('lw', 'l2', 0.033e-6); c.r('lw', 'X', 10e3)
    c.r('X', 'Y', 1e6)                                                   # R3
    c.opamp('gnd', 'X', 'Y')
    return c, 'Y'

# ---------------------------------------------------------------- helpers
def db(x): return 20 * math.log10(abs(x) + 1e-30)
def logf(n=200, f0=10.0, f1=40000.0):
    return [f0 * (f1 / f0) ** (i / (n - 1)) for i in range(n)]

def resp(builder, *args, freqs=None):
    c, out = builder(*args)
    freqs = freqs or logf()
    return freqs, [c.solve(f, [out])[0] for f in freqs]

def peak(freqs, h, sign=1):
    i = max(range(len(h)), key=lambda k: sign * db(h[k]))
    return freqs[i], db(h[i])

def bandwidth_oct(freqs, h):
    """-3 dB (relative to peak in dB above 0) width of a boost in octaves"""
    g = [db(x) for x in h]
    i = max(range(len(g)), key=lambda k: g[k])
    half = g[i] - 3.0
    lo = i
    while lo > 0 and g[lo] > half: lo -= 1
    hi = i
    while hi < len(g) - 1 and g[hi] > half: hi += 1
    return math.log2(freqs[hi] / freqs[lo])

def main():
    F = logf(400, 10, 40000)
    print("== Lo cut")
    f, h = resp(locut, freqs=F)
    for ff in (20, 40, 50, 60, 75, 100, 150, 1000):
        print("  %6.0f Hz %7.2f dB" % (ff, db(locut()[0].solve(ff, ['out'])[0])))
    m3 = min((abs(db(x) + 3), ff) for ff, x in zip(f, h))[1]
    print("  -3 dB at %.1f Hz, max %.2f dB" % (m3, max(db(x) for x in h)))

    print("== Lo mid (U2B)")
    for fr in (0.0, 0.25, 0.5, 0.75, 1.0):
        f, hb = resp(mid1, 1.0, fr, freqs=F)
        f, hc = resp(mid1, 0.0, fr, freqs=F)
        f, h0 = resp(mid1, 0.5, fr, freqs=F)
        pf, pb = peak(f, hb); cf, cb = peak(f, hc, -1)
        flat = max(abs(db(x)) for x in h0)
        print("  freq %.2f: boost %.1f dB @ %.0f Hz (bw %.2f oct), cut %.1f dB @ %.0f Hz, flat dev %.3f dB"
              % (fr, pb, pf, bandwidth_oct(f, hb), cb, cf, flat))

    print("== Hi mid (U3A + SVF) - Q pot raw: q_lower = rot")
    for q in (0.0, 0.25, 0.5, 0.75, 1.0):
        f, hb = resp(mid2, 1.0, 0.5, q, freqs=F)
        f, hc = resp(mid2, 0.0, 0.5, q, freqs=F)
        f, h0 = resp(mid2, 0.5, 0.5, q, freqs=F)
        pf, pb = peak(f, hb); cf, cb = peak(f, hc, -1)
        print("  q %.2f: boost %.1f dB @ %.0f Hz bw %.2f oct, cut %.1f dB @ %.0f Hz, flat dev %.3f"
              % (q, pb, pf, bandwidth_oct(f, hb), cb, cf, max(abs(db(x)) for x in h0)))
    for fr in (0.0, 0.2, 0.5, 0.8, 1.0):
        f, hb = resp(mid2, 1.0, fr, 0.5, freqs=F)
        pf, pb = peak(f, hb)
        print("  freq %.2f: boost %.1f dB @ %.0f Hz" % (fr, pb, pf))

    print("== Hi/Lo shelf (Baxandall, inverted)")
    for hi, lo in ((0.5, 0.5), (1, 0.5), (0, 0.5), (0.5, 1), (0.5, 0)):
        c, out = hilo(hi, lo)
        vals = [db(c.solve(ff, [out])[0]) for ff in (20, 40, 80, 160, 1000, 3000, 6000, 12000, 20000)]
        print("  hi %.1f lo %.1f: " % (hi, lo) + " ".join("%6.1f" % v for v in vals))

CASES = [
    ('locut', ()),
    ('mid1', (1.0, 0.0)), ('mid1', (0.0, 0.5)), ('mid1', (1.0, 1.0)), ('mid1', (0.75, 0.3)), ('mid1', (0.5, 0.5)),
    ('mid2', (1.0, 0.5, 0.5)), ('mid2', (0.0, 0.2, 0.9)), ('mid2', (1.0, 1.0, 0.0)), ('mid2', (0.3, 0.7, 0.75)),
    ('mid2', (1.0, 0.0, 1.0)), ('mid2', (0.5, 0.5, 0.5)),
    ('hilo', (1.0, 0.0)), ('hilo', (0.0, 1.0)), ('hilo', (0.5, 0.5)), ('hilo', (0.8, 0.3)),
]

def table(path):
    builders = {'locut': locut, 'mid1': mid1, 'mid2': mid2, 'hilo': hilo}
    freqs = logf(31, 20.0, 20000.0)
    with open(path, 'w') as fo:
        fo.write('# section p0 p1 p2 freq re im  (generated by tools/circuit_ref.py --table)\n')
        for name, args in CASES:
            c, out = builders[name](*args)
            pa = list(args) + [0.0] * (3 - len(args))
            for f in freqs:
                h = c.solve(f, [out])[0]
                fo.write('%s %.4f %.4f %.4f %.6f %.12e %.12e\n' % (name, pa[0], pa[1], pa[2], f, h.real, h.imag))
    print('written', path)

if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--table':
        table(sys.argv[2])
    else:
        main()
