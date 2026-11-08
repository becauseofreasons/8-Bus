// Saved state of the processor (inside the DAW project): all parameter values by id + tolerance seed.
// Unknown ids are skipped when reading, missing ids keep their defaults - new parameters can be added.
#pragma once
#include "ids.h"
#include "pluginterfaces/base/ibstream.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace eightbus {

struct PluginState {
  uint32_t seed = 0;
  std::vector<std::pair<uint32_t, double>> params;

  bool write(Steinberg::IBStream* s) const {
    if (!s) return false;
    auto u32 = [&](uint32_t v) { return s->write(&v, 4, nullptr) == Steinberg::kResultOk; };
    bool ok = u32(kStateMagic) && u32(kStateVersion) && u32(seed) && u32(uint32_t(params.size()));
    for (const auto& p : params) {
      ok = ok && u32(p.first);
      double v = p.second;
      ok = ok && s->write(&v, 8, nullptr) == Steinberg::kResultOk;
    }
    return ok;
  }
  bool read(Steinberg::IBStream* s) {
    if (!s) return false;
    auto u32 = [&](uint32_t& v) {
      Steinberg::int32 got = 0;
      return s->read(&v, 4, &got) == Steinberg::kResultOk && got == 4;
    };
    uint32_t magic = 0, version = 0, count = 0;
    if (!u32(magic) || magic != kStateMagic || !u32(version) || version < 1 || !u32(seed) || !u32(count)) return false;
    if (count > 10000) return false;
    params.clear();
    for (uint32_t i = 0; i < count; i++) {
      uint32_t id;
      double v;
      Steinberg::int32 got = 0;
      if (!u32(id) || s->read(&v, 8, &got) != Steinberg::kResultOk || got != 8) return false;
      if (v >= 0.0 && v <= 1.0) params.push_back({ id, v });
    }
    return true;
  }
};

} // namespace eightbus
