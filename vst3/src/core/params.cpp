#include "params.h"
#include "sections.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace r8 {

namespace {

std::vector<ParamDef> makeDefs() {
  std::vector<ParamDef> d;
  d.push_back({ kParamInGain, "Input", "In", "dB", 0.5, 0, true, false });
  d.push_back({ kParamOutGain, "Output", "Out", "dB", 0.5, 0, true, false });
  d.push_back({ kParamStrips, "Channels", "Chans", "", 0.0, kMaxStrips - 1, false, false });
  d.push_back({ kParamOversampling, "Oversampling", "OS", "", 0.5, 2, false, false });
  d.push_back({ kParamTolerance, "Tolerances", "Tol", "", 0.0, 1, false, false });
  d.push_back({ kParamBypass, "Bypass", "Byp", "", 0.0, 1, true, true });
  d.push_back({ kParamSupply, "Supply/Ground", "Supply", "", 0.5, 0, true, false });
  const double trimDef = trimUnityRot();
  for (int s = 0; s < kMaxStrips; s++) {
    const std::string p = std::to_string(s + 1) + " ";
    d.push_back({ stripParamId(s, kTrim), p + "Trim", p + "Trim", "dB", trimDef, 0, true, false });
    d.push_back({ stripParamId(s, kHmGain), p + "Hi Mid", p + "HM", "dB", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kHmFreq), p + "Hi Mid Freq", p + "HMF", "Hz", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kHmBw), p + "Hi Mid BW", p + "BW", "oct", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kLmGain), p + "Lo Mid", p + "LM", "dB", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kLmFreq), p + "Lo Mid Freq", p + "LMF", "Hz", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kHi), p + "Hi", p + "Hi", "dB", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kLo), p + "Lo", p + "Lo", "dB", 0.5, 0, true, false });
    d.push_back({ stripParamId(s, kFader), p + "Fader", p + "Fdr", "dB", kFaderUnity, 0, true, false });
    d.push_back({ stripParamId(s, kLoCut), p + "Lo Cut", p + "Cut", "", 0.0, 1, true, false });
    d.push_back({ stripParamId(s, kEqIn), p + "EQ In", p + "EQ", "", 1.0, 1, true, false });
  }
  return d;
}

std::string fmt(const char* f, double v) {
  char b[64];
  std::snprintf(b, sizeof b, f, v);
  return b;
}
std::string dbText(double db) {
  if (db <= -89.5) return "-inf dB";
  if (std::fabs(db) < 0.05) db = 0;
  return fmt(db > 0 ? "+%.1f dB" : "%.1f dB", db);
}
std::string hzText(double hz) {
  if (hz >= 1000) return fmt(hz >= 10000 ? "%.1f kHz" : "%.2f kHz", hz / 1000);
  return fmt("%.0f Hz", hz);
}
std::string octText(double oct) {
  if (oct >= 0.95) return fmt("%.2f oct", oct);
  return "1/" + fmt("%.1f", 1.0 / oct) + " oct";
}

} // namespace

const std::vector<ParamDef>& paramDefs() {
  static const std::vector<ParamDef> d = makeDefs();
  return d;
}

int paramIndex(uint32_t id) {
  if (id >= 1 && id <= uint32_t(kGlobalParams)) return int(id) - 1;
  int strip, slot;
  if (splitStripParam(id, strip, slot)) return kGlobalParams + strip * kSlots + slot;
  return -1;
}

double paramDefault(uint32_t id) {
  const int i = paramIndex(id);
  return i >= 0 ? paramDefs()[size_t(i)].def : 0.0;
}

std::string paramText(uint32_t id, double v) {
  switch (id) {
    case kParamInGain: case kParamOutGain: return dbText(gainDbOfNorm(v));
    case kParamStrips: return std::to_string(stripsOfNorm(v));
    case kParamOversampling: return std::to_string(oversamplingOfNorm(v)) + "x";
    case kParamTolerance: return discrete(v, 1) ? "on" : "off";
    case kParamBypass: return discrete(v, 1) ? "bypass" : "on";
    case kParamSupply: return v <= 0.0005 ? "ideal" : fmt("x%.2f", supplyOfNorm(v));
  }
  int strip, slot;
  if (!splitStripParam(id, strip, slot)) return "";
  switch (slot) {
    case kTrim: return dbText(trimDb(v));
    case kHmGain: return dbText(std::fabs(v - 0.5) < 1e-4 ? 0.0 : hiMidGainDb(v, 0.5, 0.5));
    case kHmFreq: return hzText(hiMidFreqHz(v));
    case kHmBw: return octText(hiMidBwOct(v));
    case kLmGain: return dbText(std::fabs(v - 0.5) < 1e-4 ? 0.0 : loMidGainDb(v, 0.5));
    case kLmFreq: return hzText(loMidFreqHz(v));
    case kHi: return dbText(std::fabs(v - 0.5) < 1e-4 ? 0.0 : hiGainDb(v));
    case kLo: return dbText(std::fabs(v - 0.5) < 1e-4 ? 0.0 : loGainDb(v));
    case kFader: return dbText(faderDb(v));
    case kLoCut: return discrete(v, 1) ? "on" : "off";
    case kEqIn: return discrete(v, 1) ? "in" : "out";
  }
  return "";
}

bool paramFromText(uint32_t id, const std::string& text, double& norm) {
  int strip, slot;
  const bool isStrip = splitStripParam(id, strip, slot);
  // switches: the words of paramText
  const bool toggle = id == kParamTolerance || id == kParamBypass || (isStrip && (slot == kLoCut || slot == kEqIn));
  if (toggle) {
    if (text == "on" || text == "in" || text == "bypass" || text == "1") { norm = 1; return true; }
    if (text == "off" || text == "out" || text == "0") { norm = 0; return true; }
    return false;
  }
  if (text.find("inf") != std::string::npos && isStrip && slot == kFader) { norm = 0; return true; }
  if (id == kParamSupply) {
    if (text == "ideal") { norm = 0; return true; }
    const char* t = text.c_str();
    char* e = nullptr;
    const double f = std::strtod(t[0] == 'x' ? t + 1 : t, &e);
    if (e == (t[0] == 'x' ? t + 1 : t)) return false;
    norm = std::sqrt(std::max(0.0, std::min(4.0, f)) / 4.0);
    return true;
  }
  char* end = nullptr;
  const double x = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) return false;
  auto clamp = [](double v) { return v < 0 ? 0.0 : v > 1 ? 1.0 : v; };
  switch (id) {
    case kParamInGain: case kParamOutGain: norm = clamp((x + 24) / 48); return true;
    case kParamStrips: norm = clamp(normOfStrips(int(x))); return true;
    case kParamOversampling: norm = x <= 2 ? 0.0 : x <= 4 ? 0.5 : 1.0; return true;
  }
  if (!isStrip) return false;
  if (slot == kHmBw) {
    double oct = x;
    if (*end == '/') oct = x / std::strtod(end + 1, nullptr);      // "1/12.0 oct"
    if (!(oct > 0)) return false;
    double lo = 0, hi = 1;                                          // bandwidth falls with the knob
    for (int i = 0; i < 40; i++) { const double m = 0.5 * (lo + hi); if (hiMidBwOct(m) > oct) lo = m; else hi = m; }
    norm = 0.5 * (lo + hi);
    return true;
  }
  // monotonic read-outs: search the pot position
  auto search = [&](auto f, double target, bool rising) {
    double lo = 0, hi = 1;
    for (int i = 0; i < 40; i++) {
      const double m = 0.5 * (lo + hi);
      if ((f(m) < target) == rising) lo = m; else hi = m;
    }
    return 0.5 * (lo + hi);
  };
  switch (slot) {
    case kTrim: norm = search([](double r) { return trimDb(r); }, x, true); return true;
    case kFader: norm = faderPosOfDb(x); return true;
    case kHmFreq: { double hz = x; if (text.find('k') != std::string::npos) hz *= 1000; norm = search([](double r) { return hiMidFreqHz(r); }, hz, true); return true; }
    case kLmFreq: { double hz = x; if (text.find('k') != std::string::npos) hz *= 1000; norm = search([](double r) { return loMidFreqHz(r); }, hz, true); return true; }
    case kHmGain: norm = search([](double r) { return hiMidGainDb(r, 0.5, 0.5); }, x, true); return true;
    case kLmGain: norm = search([](double r) { return loMidGainDb(r, 0.5); }, x, true); return true;
    case kHi: norm = search([](double r) { return hiGainDb(r); }, x, true); return true;
    case kLo: norm = search([](double r) { return loGainDb(r); }, x, true); return true;
  }
  return false;
}

} // namespace r8
