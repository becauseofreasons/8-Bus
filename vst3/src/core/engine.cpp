#include "engine.h"
#include <algorithm>
#include <cmath>
#if defined(__SSE__) || defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#define R8_HAVE_SSE 1
#endif

namespace r8 {

void Engine::OutHp::set(double fc, double fs) {
  const double k = std::tan(3.14159265358979323846 * fc / fs), q = 0.7071067811865476;
  const double n = 1 / (1 + k / q + k * k);
  b0 = n; b1 = -2 * n; b2 = n;
  a1 = 2 * (k * k - 1) * n;
  a2 = (1 - k / q + k * k) * n;
}
void Engine::OutHp::reset() {
  for (int c = 0; c < 2; c++) x1[c] = x2[c] = y1[c] = y2[c] = 0;
}

Engine::Engine() {
  const auto& defs = paramDefs();
  norm.resize(defs.size());
  smooth.resize(defs.size());
  for (size_t i = 0; i < defs.size(); i++) norm[i] = smooth[i] = defs[i].def;
  os.prepare();
}

void Engine::prepare(double sampleRate, int block) {
  fs = sampleRate > 0 ? sampleRate : 48000;
  maxBlock = std::max(block, Block);
  base.assign(size_t(Block), 0.0);
  high.assign(size_t(Block * 8), 0.0);
  for (auto& d : dry) d.assign(size_t(Oversampler::Latency), 0.0f);
  smoothCoef = 1.0 - std::exp(-double(Block) / (0.030 * fs));    // 30 ms
  outHp.set(OutHpHz, fs);
  osFactor = oversamplingOfNorm(norm[size_t(paramIndex(kParamOversampling))]);
  os.setFactor(osFactor);
  for (size_t i = 0; i < norm.size(); i++) smooth[i] = norm[i];
  needTolerance = true;
  for (int s = 0; s < kMaxStrips; s++) {
    chain[s].update(controlsOf(s), 0);
    chain[s].prepare(fs * osFactor);
  }
  for (Supply& p : supply) { p.prepare(fs * osFactor); p.setCondition(supplyOfNorm(norm[size_t(paramIndex(kParamSupply))])); }
  reset();
}

void Engine::reset() {
  os.reset();
  outHp.reset();
  for (Supply& p : supply) p.reset();
  for (Strip& s : chain) s.reset();
  for (auto& d : dry) std::fill(d.begin(), d.end(), 0.0f);
  dryPos = 0;
  inGain = std::pow(10.0, gainDbOfNorm(norm[size_t(paramIndex(kParamInGain))]) / 20);
  outGain = std::pow(10.0, gainDbOfNorm(norm[size_t(paramIndex(kParamOutGain))]) / 20);
  bypassMix = bypass ? 1 : 0;
}

void Engine::setSeed(uint32_t s) {
  if (s != toleranceSeed) { toleranceSeed = s; needTolerance = true; }
}

void Engine::applyTolerance() {
  for (int s = 0; s < kMaxStrips; s++) {
    Tolerance t;
    if (tolerances) t.randomize(toleranceSeed * 2654435761u + uint32_t(s) * 40503u + 17u);
    chain[s].setTolerance(t);
  }
  needTolerance = false;
}

double Engine::param(uint32_t id) const {
  const int i = paramIndex(id);
  return i >= 0 ? norm[size_t(i)] : 0.0;
}

void Engine::setParam(uint32_t id, double v) {
  const int i = paramIndex(id);
  if (i < 0) return;
  v = std::max(0.0, std::min(1.0, v));
  norm[size_t(i)] = v;
  switch (id) {
    case kParamStrips: {
      const int n = stripsOfNorm(v);
      for (int s = activeStrips; s < n; s++) chain[s].reset();     // a new channel starts silent
      activeStrips = n;
      break;
    }
    case kParamOversampling: {
      const int f = oversamplingOfNorm(v);
      if (f != osFactor) {
        osFactor = f;
        os.setFactor(f);
        for (Strip& s : chain) s.prepare(fs * f);
        for (Supply& p : supply) p.prepare(fs * f);
      }
      break;
    }
    case kParamTolerance: {
      const bool t = discrete(v, 1) != 0;
      if (t != tolerances) { tolerances = t; needTolerance = true; }
      break;
    }
    case kParamBypass: bypass = discrete(v, 1) != 0; break;
    case kParamSupply: for (Supply& p : supply) p.setCondition(supplyOfNorm(v)); break;
    default: {
      int strip, slot;
      if (splitStripParam(id, strip, slot) && (slot == kLoCut || slot == kEqIn)) smooth[size_t(i)] = v;   // switches: no smoothing
      break;
    }
  }
}

StripControls Engine::controlsOf(int s) const {
  auto at = [&](int slot) { return smooth[size_t(paramIndex(stripParamId(s, slot)))]; };
  StripControls c;
  c.trim = at(kTrim); c.hmGain = at(kHmGain); c.hmFreq = at(kHmFreq); c.hmBw = at(kHmBw);
  c.lmGain = at(kLmGain); c.lmFreq = at(kLmFreq); c.hi = at(kHi); c.lo = at(kLo); c.fader = at(kFader);
  c.loCut = discrete(at(kLoCut), 1) != 0;
  c.eqIn = discrete(at(kEqIn), 1) != 0;
  return c;
}

void Engine::controlStep(int n) {
  if (needTolerance) applyTolerance();
  // smooth the pots of the active strips (close enough: jump)
  for (int s = 0; s < activeStrips; s++) {
    for (int slot = 0; slot < kSlots; slot++) {
      const size_t i = size_t(paramIndex(stripParamId(s, slot)));
      double& x = smooth[i];
      const double d = norm[i] - x;
      x = std::fabs(d) < 1e-5 ? norm[i] : x + smoothCoef * d;
    }
    chain[s].update(controlsOf(s), n * osFactor);
  }
}

void Engine::process(const float* const* in, float* const* out, int nch, int n) {
#ifdef R8_HAVE_SSE
  const unsigned csr = _mm_getcsr();
  _mm_setcsr(csr | 0x8040);                 // flush denormals to zero (FTZ | DAZ)
#endif
  nch = std::min(nch, 2);
  const double inTarget = std::pow(10.0, gainDbOfNorm(norm[size_t(paramIndex(kParamInGain))]) / 20);
  const double outTarget = std::pow(10.0, gainDbOfNorm(norm[size_t(paramIndex(kParamOutGain))]) / 20);
  const int L = osFactor;
  for (int off = 0; off < n; off += Block) {
    const int m = std::min(Block, n - off);
    controlStep(m);
    const double in0 = inGain, out0 = outGain;
    const double in1 = in0 + smoothCoef * (inTarget - in0), out1 = out0 + smoothCoef * (outTarget - out0);
    const double bp0 = bypassMix, bp1 = bypass ? std::min(1.0, bp0 + double(m) / (0.02 * fs)) : std::max(0.0, bp0 - double(m) / (0.02 * fs));
    for (int c = 0; c < nch; c++) {
      const float* x = in[c] + off;
      float* y = out[c] + off;
      for (int i = 0; i < m; i++) {
        const double g = in0 + (in1 - in0) * (i + 1) / m;
        const double v = x[i] * g;
        meters.in[c] = std::max(meters.in[c], float(std::fabs(v)));
        base[size_t(i)] = v * kVoltsPerUnit;
      }
      // dry signal for bypass, delayed by the latency (read before out may overwrite in)
      float dryv[Block];
      {
        std::vector<float>& d = dry[c];
        int p = dryPos;
        for (int i = 0; i < m; i++) {
          dryv[i] = d[size_t(p)];
          d[size_t(p)] = x[i];
          p = (p + 1) % Oversampler::Latency;
        }
      }
      os.up(c, base.data(), m, high.data());
      const int nh = m * L;
      // all strips sample by sample: they share supply and ground (the loop closes through them)
      Supply& sup = supply[c];
      for (int j = 0; j < nh; j++) {
        sup.input(high[size_t(j)]);
        double v = high[size_t(j)] + sup.vg;     // the outside source sees the console ground move
        double ip = 0, in = 0, is = 0;
        for (int s = 0; s < activeStrips; s++) {
          v = chain[s].tick(c, v, sup.rails);
          double a, b;
          chain[s].currents(c, a, b);
          ip += a; in += b;
          is += chain[s].saturationCurrent(c);
        }
        sup.update(ip, in, is);
        high[size_t(j)] = v;
      }
      os.down(c, high.data(), m, base.data());
      for (int i = 0; i < m; i++) {
        const double g = out0 + (out1 - out0) * (i + 1) / m;
        const double wet = outHp.tick(c, base[size_t(i)]) / kVoltsPerUnit * g;
        const double b = bp0 + (bp1 - bp0) * (i + 1) / m;
        const float v = float(wet + b * (dryv[i] - wet));
        y[i] = v;
        meters.out[c] = std::max(meters.out[c], std::fabs(v));
      }
    }
    dryPos = (dryPos + m) % Oversampler::Latency;
    inGain = in1; outGain = out1; bypassMix = bp1;
    for (int s = 0; s < activeStrips; s++) {
      float v = 0;
      for (int c = 0; c < nch; c++) v = std::max(v, float(chain[s].olVoltage(c)));
      meters.ol[s] = std::max(meters.ol[s], v);
    }
  }
#ifdef R8_HAVE_SSE
  _mm_setcsr(csr);
#endif
}

void Engine::takeMeters(Meters& m) {
  m = meters;
  m.strips = activeStrips;
  meters = Meters();
}

} // namespace r8
