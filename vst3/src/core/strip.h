// One channel strip of the console at the oversampled rate, in volts.
// Signal path (board 08 + 09):
//   line in -> pre-amp (TRIM, U1A) -> [LO CUT, U104-22] -> [EQ: lo mid U2B -> hi mid U3A -> hi/lo U1B, U2A]
//   -> fader -> U102B (+10 dB) -> direct out
// Every op-amp output can clip at its supply rail; between the clip points the filters are the exact
// circuits (see sections.h). Two audio channels share the coefficients.
#pragma once
#include "circuit.h"
#include "sections.h"
#include <cmath>

namespace r8 {

struct StripControls {
  double trim = 0.17, hmGain = 0.5, hmFreq = 0.5, hmBw = 0.5, lmGain = 0.5, lmFreq = 0.5, hi = 0.5, lo = 0.5,
         fader = kFaderUnity;
  bool loCut = false, eqIn = true;
};

// input pair of the pre-amp: the current through the gain resistor runs into the bias current
// (one half cuts off); x = current / bias
inline double pairLimit(double x) {
  constexpr double knee = 0.8, w = 1 - knee;
  const double a = std::fabs(x);
  if (a <= knee) return x;
  const double y = knee + w * std::tanh((a - knee) / w);
  return x < 0 ? -y : y;
}

// output stage of a 4560 near its rails: linear up to the knee, then bends into the rail
inline double opampClip(double v, double pos, double neg) {
  constexpr double knee = 0.92;
  if (v > knee * pos) {
    const double w = (1 - knee) * pos;
    return knee * pos + w * std::tanh((v - knee * pos) / w);
  }
  if (v < -knee * neg) {
    const double w = (1 - knee) * neg;
    return -knee * neg - w * std::tanh((-v - knee * neg) / w);
  }
  return v;
}

struct Hp1 {                      // first order high pass (coupling capacitors)
  double b0 = 1, a1 = 0;
  void set(double fc, double fs) {
    const double k = std::tan(kPi * fc / fs);
    b0 = 1 / (1 + k);
    a1 = (1 - k) / (1 + k);
  }
  struct State { double x1 = 0, y1 = 0; };
  inline double tick(State& s, double x) const {
    const double y = b0 * (x - s.x1) + a1 * s.y1;
    s.x1 = x; s.y1 = y;
    return y;
  }
};

// supply rails as the channel sees them right now (they sag under load, see supply.h)
struct Rails {
  double pos = 0, neg = 0;          // sag of +18 V / -18 V (volts, >= 0)
  double prePos = 0, preNeg = 0;    // same for the +-16 V of the pre-amps
};

class Strip {
public:
  static constexpr int MaxCh = 2;
  // rails of the op-amp outputs (volts): pre-amp on +-16 V, channel board on +-18 V
  static constexpr double PreRailPos = 14.6, PreRailNeg = 14.4;
  static constexpr double RailPos = 16.1, RailNeg = 15.9;
  static constexpr double OlThreshold = 2.78;      // comparator U101A (R120/R121/R122 from +18 V)
  // load of each op-amp output (siemens): feedback network + next stage, from the schematic
  static constexpr double LoadPre = 1 / 2.5e3, LoadLoCut = 1 / 2.0e3, LoadLoMid = 1 / 3.3e3, LoadHiMid = 1 / 4.0e3,
                          LoadHiLo = 1 / 4.0e3, LoadInv = 1 / 4.0e3, LoadFader = 1 / 1.4e3;

  void prepare(double fsOversampled);              // coefficients + reset (no allocation)
  void reset();
  void setTolerance(const Tolerance& t);           // rebuilds everything
  // new control values; ramps of the gains run over the next rampSamples (oversampled) samples
  void update(const StripControls& c, int rampSamples);

  inline double tick(int ch, double v, const Rails& rails);
  // current the strip's op-amps pushed into their loads in the last sample (amperes):
  // drawn from +18 V, from -18 V, and returned through ground (signed)
  void currents(int ch, double& fromPos, double& fromNeg) const { fromPos = st[ch].iPos; fromNeg = st[ch].iNeg; }
  // extra current of op-amps driven into saturation (signed, amperes): the feedback opens, driver and
  // output stage pull peak current (limited to about +-20 mA per op-amp). Zero while everything is linear.
  double saturationCurrent(int ch) const { return st[ch].iSat; }
  static constexpr double SatCurrentMax = 0.020;

  double olVoltage(int ch) const { return st[ch].olCap; }
  // last stage outputs: 0 pre-amp, 1 lo mid, 2 hi mid, 3 hi/lo (U1B), 4 EQ out, 5 fader stage
  const double* taps(int ch) const { return st[ch].tap; }
  const StripControls& controls() const { return cur; }
  double fs() const { return rate; }

private:
  double rate = 192000;
  Tolerance tol;
  StripControls cur;
  bool built = false;

  DkFilter<kLoCutCaps> loCut;
  DkFilter<kLoMidCaps> loMid;
  DkFilter<kHiMidCaps> hiMid;
  DkFilter<kHiLoCaps> hiLo;
  Hp1 hpIn, hpCouple, hpFader;
  double c107Step = 0;                            // 1 / (C107 * fs)

  double gPreInc = 0, gFaderInc = 0;              // ramps of 1/Rg and the fader gain (same for both channels)
  double xfStep = 0.001;                          // switch cross fades (10 ms)

  struct Ch {
    DkFilter<kLoCutCaps>::State lc;
    DkFilter<kLoMidCaps>::State lm;
    DkFilter<kHiMidCaps>::State hm;
    DkFilter<kHiLoCaps>::State hl;
    Hp1::State in, pre, lcOut, mid, eq, fader;
    double olCap = 0;
    double vc107 = 0;                              // voltage across C107 (pre-amp gain network)
    double tap[7] = {};                            // stage outputs of the last sample (tests, meters, currents)
    double iPos = 0, iNeg = 0, iSat = 0;
    double gPre = 1, gFader = 1, xfLoCut = 0, xfEq = 1;   // gPre = 1 / Rg
    int ramp = 0;
  } st[MaxCh];

  void rebuildLoCut();
  void rebuildLoMid();
  void rebuildHiMid();
  void rebuildHiLo();
};

inline double Strip::tick(int ch, double v, const Rails& r) {
  Ch& s = st[ch];
  double sat = 0;
  // op-amp output with its rails; the overdrive (wanted - got) times the load gives the saturation current
  auto clip = [&](double want, double p, double n, double load) {
    const double got = opampClip(want, p, n);
    const double d = (want - got) * load;
    if (d != 0) sat += SatCurrentMax * std::tanh(d / SatCurrentMax);
    return got;
  };
  const double preP = PreRailPos - r.prePos, preN = PreRailNeg - r.preNeg;
  const double rP = RailPos - r.pos, rN = RailNeg - r.neg;
  // bias of the input pair: 2K49 from the +16 V rail
  const double bias = kPreampBias * (1.0 - r.prePos / 15.35);
  if (s.ramp > 0) { s.gPre += gPreInc; s.gFader += gFaderInc; s.ramp--; }
  // ---- pre-amp: input coupling, input pair with the gain network Rg + C107, U1A.
  // C107 charges only through the current of the pair: when the pair is overdriven it barely moves,
  // so a hot kick does not leave a big charge behind.
  v = hpIn.tick(s.in, v);
  double i = (kPreampPad * v - s.vc107) * s.gPre;
  i = bias * pairLimit(i / bias);
  s.vc107 += i * c107Step;
  v = clip(kPreampTransimpedance * i, preP, preN, LoadPre);
  v = hpCouple.tick(s.pre, v);
  const double tapPre = s.tap[0] = v;
  // ---- lo cut (cross fade with the switch)
  const double fLc = s.xfLoCut = cur.loCut ? std::min(1.0, s.xfLoCut + xfStep) : std::max(0.0, s.xfLoCut - xfStep);
  if (fLc > 0) {
    double lc = loCut.tick(s.lc, v);
    lc = s.tap[6] = clip(lc, rP, rN, LoadLoCut);
    lc = hpCouple.tick(s.lcOut, lc);
    v += fLc * (lc - v);
  } else {
    s.lc = {};
    s.tap[6] = 0;
  }
  // ---- EQ: lo mid (U2B), hi mid (U3A), hi/lo Baxandall (U1B) + inverter (U2A)
  const double fEq = s.xfEq = cur.eqIn ? std::min(1.0, s.xfEq + xfStep) : std::max(0.0, s.xfEq - xfStep);
  double tapEq = v;
  if (fEq > 0) {
    double a = s.tap[1] = clip(loMid.tick(s.lm, v), rP, rN, LoadLoMid);
    double b = s.tap[2] = clip(hiMid.tick(s.hm, a), rP, rN, LoadHiMid);
    b = hpCouple.tick(s.mid, b);                                   // C112 / R114
    double y = s.tap[3] = clip(hiLo.tick(s.hl, b), rP, rN, LoadHiLo);    // inverted
    double z = clip(-y, rP, rN, LoadInv);
    z = hpCouple.tick(s.eq, z);                                    // C109 / R108
    tapEq = s.tap[4] = z;
    v += fEq * (z - v);
  } else {
    s.lm = {}; s.hm = {}; s.hl = {};
    s.tap[1] = s.tap[2] = s.tap[3] = s.tap[4] = 0;
  }
  // ---- fader + U102B (+10 dB), direct out
  v = s.tap[5] = clip(s.gFader * v, rP, rN, LoadFader);
  v = hpFader.tick(s.fader, v);
  // ---- OL LED: three peak detectors (1N4148 + 200K) into C113 0.22 uF || R119 100K
  {
    double i = 0;
    const double taps[3] = { tapPre, tapEq, v };
    for (double t : taps) {
      const double d = t - 0.6 - s.olCap;
      if (d > 0) i += d / 200e3;
    }
    s.olCap += (i - s.olCap / 100e3) / (rate * 0.22e-6);
  }
  // ---- load currents of the op-amp outputs (positive half from +18 V, negative half from -18 V)
  {
    const double g[7] = { LoadPre, LoadLoMid, LoadHiMid, LoadHiLo, LoadInv, LoadFader, LoadLoCut };
    double ip = 0, in = 0;
    for (int k = 0; k < 7; k++) {
      const double c = s.tap[k] * g[k];
      if (c > 0) ip += c; else in -= c;
    }
    s.iPos = ip; s.iNeg = in;
    s.iSat = sat;
  }
  return v;
}

} // namespace r8
