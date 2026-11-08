#include "strip.h"
#include <algorithm>

namespace r8 {

void Strip::prepare(double fsOversampled) {
  rate = fsOversampled;
  hpIn.set(0.86, rate);           // C109/C110 47 uF into 3K92
  hpCouple.set(0.5, rate);        // 47 uF couplings into the next stage (about 0.2 .. 0.7 Hz)
  hpFader.set(0.23, rate);        // C132 47 uF into R174 15K
  c107Step = 1.0 / (kPreampC107 * rate);
  xfStep = 1.0 / (0.010 * rate);
  built = false;
  update(cur, 0);
  reset();
}

void Strip::reset() {
  for (Ch& c : st) {
    c = Ch();
    c.gPre = 1.0 / preampRg(cur.trim);
    c.gFader = std::pow(10.0, faderDb(cur.fader) / 20.0);
    c.xfLoCut = cur.loCut ? 1 : 0;
    c.xfEq = cur.eqIn ? 1 : 0;
  }
}

void Strip::setTolerance(const Tolerance& t) {
  tol = t;
  built = false;
  update(cur, 0);
}

void Strip::rebuildLoCut() { Netlist n; buildLoCut(n, tol); discretize(n, rate, loCut); }
void Strip::rebuildLoMid() { Netlist n; buildLoMid(n, cur.lmGain, cur.lmFreq, tol); discretize(n, rate, loMid); }
void Strip::rebuildHiMid() { Netlist n; buildHiMid(n, cur.hmGain, cur.hmFreq, cur.hmBw, tol); discretize(n, rate, hiMid); }
void Strip::rebuildHiLo() { Netlist n; buildHiLo(n, cur.hi, cur.lo, tol); discretize(n, rate, hiLo); }

void Strip::update(const StripControls& c, int rampSamples) {
  auto changed = [](double a, double b) { return std::abs(a - b) > 1e-9; };
  const bool all = !built;
  const StripControls old = cur;
  cur = c;
  if (all) rebuildLoCut();
  if (all || changed(old.lmGain, c.lmGain) || changed(old.lmFreq, c.lmFreq)) rebuildLoMid();
  if (all || changed(old.hmGain, c.hmGain) || changed(old.hmFreq, c.hmFreq) || changed(old.hmBw, c.hmBw)) rebuildHiMid();
  if (all || changed(old.hi, c.hi) || changed(old.lo, c.lo)) rebuildHiLo();

  const double gP = 1.0 / preampRg(c.trim);
  const double gF = std::pow(10.0, faderDb(c.fader) / 20.0);
  if (all || rampSamples <= 0) {
    for (Ch& s : st) { s.gPre = gP; s.gFader = gF; s.ramp = 0; }
    gPreInc = gFaderInc = 0;
  } else {
    // both channels hold the same gains at block boundaries
    gPreInc = (gP - st[0].gPre) / rampSamples;
    gFaderInc = (gF - st[0].gFader) / rampSamples;
    for (Ch& s : st) { s.gPre = st[0].gPre; s.gFader = st[0].gFader; s.ramp = rampSamples; }
  }
  built = true;
}

} // namespace r8
