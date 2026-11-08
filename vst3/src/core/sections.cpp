#include "sections.h"
#include <algorithm>
#include <cmath>

namespace r8 {

// ------------------------------------------------------------------------------- tolerances
void Tolerance::randomize(uint32_t seed) {
  // xorshift32 + Box-Muller, deterministic per seed
  uint32_t s = seed ? seed : 0x9E3779B9u;
  auto next = [&]() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s + 0.5) / 4294967296.0; };
  auto gauss = [&]() { double u = next(), v = next(); return std::sqrt(-2 * std::log(u)) * std::cos(2 * kPi * v); };
  auto clampSigma = [](double g) { return std::max(-2.5, std::min(2.5, g)); };
  for (double& v : r) v = 1 + 0.006 * clampSigma(gauss());     // 1 % parts (2.5 sigma = 1.5 %)
  for (double& v : c) v = 1 + 0.03 * clampSigma(gauss());      // 5 % parts
  for (double& v : gang) v = 1 + 0.015 * clampSigma(gauss());  // tracking of the dual pots
}

double taperLog(double x, double mid) {
  x = std::max(0.0, std::min(1.0, x));
  const double a = (1.0 / mid - 1.0) * (1.0 / mid - 1.0);
  return (std::pow(a, x) - 1.0) / (a - 1.0);
}

static double clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

// ------------------------------------------------------------------------------- lo cut
// 3rd order Sallen-Key high pass (Chebyshev, 75 Hz), buffer U104-22/23 (U1A on the module)
void buildLoCut(Netlist& n, const Tolerance& t) {
  n = Netlist();
  const int in = n.node(), a = n.node(), b = n.node(), p = n.node(), out = n.node();
  n.in = in; n.out = out;
  n.c(in, a, 0.22e-6 * t.c[0]);           // C101
  n.r(a, GND, 5.1e3 * t.r[0]);            // R101
  n.c(a, b, 0.22e-6 * t.c[1]);            // C102
  n.r(b, out, 1.3e3 * t.r[1]);            // R102
  n.c(b, p, 0.22e-6 * t.c[2]);            // C103
  n.r(p, GND, 62e3 * t.r[2]);             // R103
  n.opamp(p, out, out);                   // follower
}

// ------------------------------------------------------------------------------- lo mid
// U2B (U104-15): inverting 10K/10K, the + input gets the Wien band pass of the R112 wiper
void buildLoMid(Netlist& n, double gain, double freq, const Tolerance& t) {
  n = Netlist();
  gain = clamp01(gain);
  const int in = n.node(), nm = n.node(), A = n.node(), P = n.node(), w = n.node(),
            s1 = n.node(), s2 = n.node(), s3 = n.node();
  n.in = in; n.out = A;
  n.r(in, nm, 10e3 * t.r[3]);             // R29
  n.r(nm, A, 10e3 * t.r[4]);              // R30
  n.opamp(P, nm, A);
  n.r(in, w, gain * 10e3);                // R112 10KB, wiper toward A = boost
  n.r(w, A, (1 - gain) * 10e3);
  const double rf = 500e3 * taperLog(1.0 - clamp01(freq));   // R111A/B 500KC
  n.c(w, s1, 0.01e-6 * t.c[3]);           // C110
  n.r(s1, s2, 8.2e3 * t.r[5]);            // R109
  n.r(s2, P, rf);                         // R111A
  n.r(P, s3, rf * t.gang[0]);             // R111B
  n.r(s3, GND, 8.2e3 * t.r[6]);           // R110
  n.c(P, GND, 3.9e-9 * t.c[4]);           // C111
}

// ------------------------------------------------------------------------------- hi mid
static double q_of_lower(double rlow) {
  const double rup = std::max(1.0, 50e3 - rlow);
  const double k = (1 / 27e3) / (1 / 11e3 + 1 / 27e3 + 1 / (160.0 + rlow));
  const double rn2 = 2.7e3 + 1 / (1 / 2.7e3 + 1 / rup);
  const double g = 2 + 27e3 / rn2;
  return 1 / (k * g);
}
static double bw_of_q(double q) { return 2 * std::asinh(1 / (2 * q)) / std::log(2.0); }
static double bw_target(double rot) {
  const double l0 = std::log2(3.0), l1 = 1.0, l2 = std::log2(1.0 / 12);
  rot = clamp01(rot);
  const double l = rot < 0.5 ? l0 + (l1 - l0) * rot / 0.5 : l1 + (l2 - l1) * (rot - 0.5) / 0.5;
  return std::pow(2.0, l);
}

double hiMidLowerOhm(double bw) {
  const double target = bw_target(bw);
  double lo = 0, hi = 50e3;
  for (int i = 0; i < 48; i++) {
    const double mid = 0.5 * (lo + hi);
    if (bw_of_q(q_of_lower(mid)) < target) lo = mid; else hi = mid;
  }
  return 0.5 * (lo + hi);
}
double hiMidQ(double bw) { return q_of_lower(hiMidLowerOhm(bw)); }
double hiMidBwOct(double bw) { return bw_target(bw); }

// U3A (U104-4): inverting 10K/10K, + input = band pass of the state variable filter (U4B, U3B, U4A),
// which is driven from the R113 wiper through C139 and R16 11K
void buildHiMid(Netlist& n, double gain, double freq, double bw, const Tolerance& t) {
  n = Netlist();
  gain = clamp01(gain);
  const int in = n.node(), m = n.node(), B = n.node(), q = n.node(), w = n.node(), tt = n.node(),
            N1 = n.node(), N2 = n.node(), P7 = n.node(), P8 = n.node(), HP = n.node(), BP = n.node(),
            LP = n.node(), r1 = n.node(), n3 = n.node(), r2 = n.node(), n4 = n.node();
  n.in = in; n.out = B;
  n.r(in, m, 10e3 * t.r[7]);              // R25
  n.r(m, B, 10e3 * t.r[8]);               // R24
  n.opamp(q, m, B);                       // U3A
  n.r(BP, q, 3.6e3 * t.r[9]);             // R20 (3K6 module / 3K3 board), carries no current
  // the band pass is inverted: the wiper toward the input end boosts
  n.r(in, w, (1 - gain) * 50e3);          // R113 50KB
  n.r(w, B, gain * 50e3);
  n.c(w, tt, 0.018e-6 * t.c[5]);          // C139
  n.r(tt, N1, 11e3 * t.r[10]);            // R16
  const double rlow = hiMidLowerOhm(bw);  // R115 50KG, wiper to ground
  n.r(N1, P7, 160.0 * t.r[11]);           // R28
  n.r(P7, GND, rlow);
  n.r(P8, GND, 50e3 - rlow);
  n.r(P8, GND, 2.7e3 * t.r[12]);          // R34
  n.r(P8, N2, 2.7e3 * t.r[13]);           // R31
  n.r(BP, N1, 27e3 * t.r[14]);            // R19
  n.opamp(N1, N2, HP);                    // U4B
  n.r(N2, HP, 27e3 * t.r[15]);            // R15
  n.r(N2, LP, 27e3 * t.r[16]);            // R21
  const double rf = 50e3 * taperLog(1.0 - clamp01(freq));    // R164A/B 50KC
  n.r(HP, r1, rf);
  n.r(r1, n3, 1.3e3 * t.r[17]);           // R17
  n.c(n3, BP, 6.8e-9 * t.c[6]);           // C118
  n.opamp(GND, n3, BP);                   // U3B
  n.r(BP, r2, rf * t.gang[1]);
  n.r(r2, n4, 1.3e3 * t.r[18]);           // R18
  n.c(n4, LP, 6.8e-9 * t.c[7]);           // C119
  n.opamp(GND, n4, LP);                   // U4A
}

// ------------------------------------------------------------------------------- hi / lo shelf
// Baxandall around U1B (U104-18..21): output inverted, U2A (U104-24) inverts again
void buildHiLo(Netlist& n, double hi, double lo, const Tolerance& t) {
  n = Netlist();
  hi = clamp01(hi); lo = clamp01(lo);
  const int in = n.node(), Y = n.node(), X = n.node(), h1 = n.node(), h2 = n.node(), hw = n.node(),
            l1 = n.node(), l2 = n.node(), lw = n.node();
  n.in = in; n.out = Y;
  n.c(in, h1, 6.8e-9 * t.c[8]);           // C105
  n.r(h1, hw, (1 - hi) * 50e3);           // R106 50KB, wiper toward the input = boost
  n.r(hw, h2, hi * 50e3);
  n.c(h2, Y, 6.8e-9 * t.c[9]);            // C106
  n.r(hw, X, 3.9e3 * t.r[19]);            // R7
  n.r(in, l1, 5.6e3 * t.r[20]);           // R105
  n.r(l1, lw, (1 - lo) * 50e3);           // R107 50KB
  n.r(lw, l2, lo * 50e3);
  n.r(l2, Y, 5.6e3 * t.r[21]);            // R4
  n.c(l1, lw, 0.033e-6 * t.c[10]);        // C107
  n.c(lw, l2, 0.033e-6 * t.c[11]);        // C108
  n.r(lw, X, 10e3 * t.r[22]);             // R2
  n.r(X, Y, 1e6 * t.r[23]);               // R3
  n.opamp(GND, X, Y);
}

// ------------------------------------------------------------------------------- read-outs
double loMidFreqHz(double rot) {
  const double r = 8.2e3 + 500e3 * taperLog(1.0 - clamp01(rot));
  return 1.0 / (2 * kPi * r * std::sqrt(0.01e-6 * 3.9e-9));
}
double hiMidFreqHz(double rot) {
  const double r = 1.3e3 + 50e3 * taperLog(1.0 - clamp01(rot));
  return 1.0 / (2 * kPi * r * 6.8e-9);
}

static const Tolerance& nominal() { static const Tolerance t; return t; }

double loMidGainDb(double gain, double freq) {
  Netlist n;
  buildLoMid(n, gain, freq, nominal());
  return 20 * std::log10(std::abs(response(n, loMidFreqHz(freq))) + 1e-12);
}
double hiMidGainDb(double gain, double freq, double bw) {
  Netlist n;
  buildHiMid(n, gain, freq, bw, nominal());
  return 20 * std::log10(std::abs(response(n, hiMidFreqHz(freq))) + 1e-12);
}
double hiGainDb(double hi) {
  Netlist n;
  buildHiLo(n, hi, 0.5, nominal());
  return 20 * std::log10(std::abs(response(n, 12000.0)) + 1e-12);
}
double loGainDb(double lo) {
  Netlist n;
  buildHiLo(n, 0.5, lo, nominal());
  return 20 * std::log10(std::abs(response(n, 80.0)) + 1e-12);
}

// ------------------------------------------------------------------------------- pre-amp
// Line input: 7K32 series / 3K92 to ground per leg. The differential pair (Q101..Q104) puts the input
// across the gain resistor R113 22R + R166 TRIM (+ about 3 ohm in the transistors), C107 470 uF in
// series; the signal current is turned into the output voltage by U1A (2 x 3K92).
static constexpr double kRgMin = 22.0 + 2.8;

double preampRg(double rot) { return kRgMin + 5000.0 * taperLog(1.0 - clamp01(rot)); }

PreampGain preampGain(double trimRot) {
  const double rg = preampRg(trimRot);
  return { kPreampPad * kPreampTransimpedance / rg, 1.0 / (2 * kPi * rg * kPreampC107) };
}
double trimDb(double trimRot) { return 20 * std::log10(preampGain(trimRot).gain); }
double trimUnityRot() {
  double lo = 0, hi = 1;
  for (int i = 0; i < 40; i++) {
    const double m = 0.5 * (lo + hi);
    if (trimDb(m) < 0) lo = m; else hi = m;
  }
  return 0.5 * (lo + hi);
}

// ------------------------------------------------------------------------------- fader
// scale of the channel fader: position -> dB (including the +10 dB of U102B)
static const double kFaderPos[] = { 0.0, 0.04, 0.08, 0.15, 0.25, 0.37, 0.52, 0.625, 0.75, 0.875, 1.0 };
static const double kFaderDb[] = { -90, -60, -50, -40, -30, -20, -10, -5, 0, 5, 10 };

double faderDb(double pos) {
  pos = clamp01(pos);
  if (pos <= 0) return -200;
  for (int i = 1; i < 11; i++)
    if (pos <= kFaderPos[i]) {
      const double f = (pos - kFaderPos[i - 1]) / (kFaderPos[i] - kFaderPos[i - 1]);
      return kFaderDb[i - 1] + f * (kFaderDb[i] - kFaderDb[i - 1]);
    }
  return 10;
}
double faderPosOfDb(double db) {
  if (db <= -90) return 0;
  for (int i = 1; i < 11; i++)
    if (db <= kFaderDb[i]) {
      const double f = (db - kFaderDb[i - 1]) / (kFaderDb[i] - kFaderDb[i - 1]);
      return kFaderPos[i - 1] + f * (kFaderPos[i] - kFaderPos[i - 1]);
    }
  return 1;
}

} // namespace r8
