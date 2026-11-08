// Shared supply and ground of the console (power supply board 15, cable, power distribution).
//
// The +-18 V are regulated inside the external supply (MJE3055 pass transistors, current limit ~2.5 A,
// 22,000 uF reservoir): plenty of current in reserve, but the discrete regulator is slow (C33 47 uF,
// C9 470 pF in its loop). Above its loop bandwidth it looks like an inductance; together with the
// decoupling capacitors on all boards (24 channels x 470 uF ~ 10 mF) the supply impedance has a
// resonance in the bass: roughly 0.8 mH with 10 mF -> ~56 Hz. Cable, connectors and ground bus add
// their resistance. Every op-amp output pushes its load current through that impedance:
//   - the rails sag with the current drawn from them (clip levels, bias of the pre-amp input pair)
//   - the ground return lifts the console ground against the outside world; the first channel's line
//     input (fed from outside) sees that as input and amplifies it with its TRIM.
// The load currents and, much stronger, the peak currents of op-amps driven into saturation (their
// feedback opens, driver and output stage pull current) shake supply and ground: a hot kick keeps ringing
// at the supply resonance ("flutter"), with more gain it holds until the next hit.
// The disturbance is always a fraction of what the incoming signal makes the console draw: the ground
// voltage reaching the input is bounded by the level of what comes in. So it can never start from silence
// and never run on by itself - without input it dies away ("flutters out"), like the original.
//
// The strength of the coupling is not in any schematic. It is calibrated to a recording of the real
// console (5 channels in a chain: one hot TRIM -> long ring, a second one -> oscillates through,
// always at ~54 Hz). "condition" scales it: 0 = ideal supply, 1 = typical (that recording), 4 = poor.
#pragma once
#include "strip.h"
#include <algorithm>
#include <cmath>

namespace r8 {

class Supply {
public:
  static constexpr double RRail = 0.15;          // ohm per rail (cable, connectors, distribution)
  static constexpr double IdleCurrent = 1.0;     // ampere drawn by the rest of the console per rail
  static constexpr double RailHz = 350;          // rail ripple smoothed by the board capacitors
  static constexpr double Coupling = 8.0;        // ohm: ground voltage at the input per ampere of load current
  static constexpr double CouplingSat = 8.0;     // ohm per ampere of saturation current
  static constexpr double InputShare = 0.3;      // ground voltage at most this share of the input level
  static constexpr double EnvRelease = 0.2;      // seconds: how long the input "feeds" the flutter
  static constexpr double EnvGate = 1e-3;        // volts: below this the input no longer feeds anything
  static constexpr double ResonanceHz = 52.0;    // supply impedance resonance (regulator + board capacitors)
  static constexpr double ResonanceQ = 2.0;

  void prepare(double fs) {
    const double pi = 3.14159265358979323846;
    aRail = 1 - std::exp(-2 * pi * RailHz / fs);
    envRel = std::exp(-1 / (EnvRelease * fs));
    // band pass (state variable filter, trapezoidal), gain 1 at the resonance
    const double g = std::tan(pi * ResonanceHz / fs), k = 1 / ResonanceQ;
    a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2; bpGain = k;
    reset();
  }
  void reset() { ic1 = ic2 = 0; vg = 0; env = 0; settle(); }
  void setCondition(double c) { cond = c; settle(); }

  // level of the outside signal at the first input (volts), before the ground voltage is added
  inline void input(double v) {
    const double a = std::fabs(v);
    env = a > env ? a : env * envRel;
    if (env < EnvGate) env = 0;
  }
  // currents of the last sample (amperes) -> rails and ground voltage for the next sample
  inline void update(double iPos, double iNeg, double iSat) {
    const double tp = cond * RRail * (IdleCurrent + iPos), tn = cond * RRail * (IdleCurrent + iNeg);
    sagP += aRail * (tp - sagP);
    sagN += aRail * (tn - sagN);
    rails.pos = rails.prePos = sagP;
    rails.neg = rails.preNeg = sagN;
    const double x = cond * (Coupling * (iPos - iNeg) + CouplingSat * iSat);
    const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2 * v1 - ic1;
    ic2 = 2 * v2 - ic2;
    const double lim = InputShare * env;
    vg = std::max(-lim, std::min(lim, bpGain * v1));
  }

  Rails rails;          // for all strips
  double vg = 0;        // ground voltage seen at the first line input (volts)

private:
  double cond = 1, aRail = 0.01;
  double a1 = 0, a2 = 0, a3 = 0, bpGain = 0, ic1 = 0, ic2 = 0;
  double sagP = 0, sagN = 0, env = 0, envRel = 0.99;
  void settle() {       // idle sag without signal
    sagP = sagN = cond * RRail * IdleCurrent;
    rails.pos = rails.prePos = sagP;
    rails.neg = rails.preNeg = sagN;
  }
};

} // namespace r8
