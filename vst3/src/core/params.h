// Parameters of the plugin (ids, names, defaults, read-outs). Shared by the engine, the VST3
// controller and the editor. Ids never change: DAW projects refer to them.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace r8 {

constexpr int kMaxStrips = 8;

enum Slot : int { kTrim, kHmGain, kHmFreq, kHmBw, kLmGain, kLmFreq, kHi, kLo, kFader, kLoCut, kEqIn, kSlots };

enum : uint32_t {
  kParamInGain = 1,
  kParamOutGain = 2,
  kParamStrips = 3,
  kParamOversampling = 4,
  kParamTolerance = 5,
  kParamBypass = 6,
  kParamSupply = 7,        // condition of supply and ground ("flutter")
};
constexpr int kGlobalParams = 7;

inline uint32_t stripParamId(int strip, int slot) { return uint32_t(100 + 20 * strip + slot); }
inline bool splitStripParam(uint32_t id, int& strip, int& slot) {
  if (id < 100 || id >= uint32_t(100 + 20 * kMaxStrips)) return false;
  strip = int(id - 100) / 20;
  slot = int(id - 100) % 20;
  return slot < kSlots;
}

struct ParamDef {
  uint32_t id;
  std::string name, shortName, unit;
  double def;          // normalized
  int steps;           // 0 = continuous, else number of steps (VST3 stepCount)
  bool automate;
  bool bypass;
};

const std::vector<ParamDef>& paramDefs();
int paramIndex(uint32_t id);                     // position in paramDefs(), -1 if unknown
double paramDefault(uint32_t id);
std::string paramText(uint32_t id, double norm); // read-out for the DAW and the editor
bool paramFromText(uint32_t id, const std::string& text, double& norm);

// discrete values (VST3 rule: value = min(max, int(norm * (max + 1))))
inline int discrete(double norm, int max) { int v = int(norm * (max + 1)); return v < 0 ? 0 : v > max ? max : v; }
inline double gainDbOfNorm(double norm) { return -24.0 + 48.0 * norm; }      // input / output gain
inline int stripsOfNorm(double norm) { return 1 + discrete(norm, kMaxStrips - 1); }
inline double normOfStrips(int n) { return double(n - 1) / (kMaxStrips - 1); }
inline int oversamplingOfNorm(double norm) { return 2 << discrete(norm, 2); }  // 2, 4, 8
inline double supplyOfNorm(double norm) { return 4.0 * norm * norm; }          // 0 ideal, 1 typical (0.5), 4 poor

} // namespace r8
