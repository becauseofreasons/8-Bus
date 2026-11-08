#pragma once
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace eightbus {

// class ids - never change them, DAW projects refer to them
static const Steinberg::FUID ProcessorUID(0x706C8901, 0x6EE443E8, 0xAED89493, 0x54722840);
static const Steinberg::FUID ControllerUID(0x7B81E3D7, 0x70334171, 0x95898E39, 0xB50EA302);

constexpr const char* kVendor = "BecauseOfReasons";
constexpr const char* kPluginName = "8-Bus";
constexpr const char* kVersion = "1.0.0";

// processor state (little endian): "R8EQ", u32 version, u32 tolerance seed, u32 count, count x (u32 id, f64 value)
constexpr Steinberg::uint32 kStateMagic = 0x51453852;   // "R8EQ"
constexpr Steinberg::uint32 kStateVersion = 1;
// controller state (editor options): "R8UI", u32 version, f64 zoom, u32 unused (0), u32 selected channel
constexpr Steinberg::uint32 kUiMagic = 0x49553852;      // "R8UI"
constexpr Steinberg::uint32 kUiVersion = 2;      // 2: + analyzer mode, speed, range, tilt, peak (u32 each)

} // namespace eightbus
