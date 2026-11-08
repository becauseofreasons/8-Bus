#include "controller.h"
#include "editor.h"
#include "ids.h"
#include "state.h"
#include "params.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <algorithm>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace eightbus {

namespace {

void toU16(const std::string& s, String128 out) { UString(out, 128).fromAscii(s.c_str()); }

// parameter with the read-outs of the core (dB, Hz, octaves)
class RParam : public Parameter {
public:
  RParam(const r8::ParamDef& d, UnitID unit) {
    info.id = d.id;
    toU16(d.name, info.title);
    toU16(d.shortName, info.shortTitle);
    toU16(d.unit, info.units);
    info.stepCount = d.steps;
    info.defaultNormalizedValue = d.def;
    info.unitId = unit;
    info.flags = (d.automate ? ParameterInfo::kCanAutomate : 0) | (d.bypass ? ParameterInfo::kIsBypass : 0) |
                 (d.steps > 1 ? ParameterInfo::kIsList : 0);
    setNormalized(d.def);
  }
  void toString(ParamValue v, String128 s) const SMTG_OVERRIDE {
    std::string t = r8::paramText(info.id, v);
    const size_t unitPos = t.rfind(' ');            // the DAW shows the unit itself
    if (unitPos != std::string::npos && t.compare(unitPos + 1, std::string::npos, "dB") == 0) t.resize(unitPos);
    toU16(t, s);
  }
  bool fromString(const TChar* s, ParamValue& v) const SMTG_OVERRIDE {
    char a[128];
    UString(const_cast<TChar*>(s), 128).toAscii(a, 128);
    double n;
    if (!r8::paramFromText(info.id, a, n)) return false;
    v = n;
    return true;
  }
};

} // namespace

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
  tresult r = EditControllerEx1::initialize(context);
  if (r != kResultOk) return r;
  for (int s = 0; s < r8::kMaxStrips; s++) {
    String128 name;
    toU16("Kanal " + std::to_string(s + 1), name);
    addUnit(new Unit(name, UnitID(s + 1), kRootUnitId));
  }
  for (const r8::ParamDef& d : r8::paramDefs()) {
    int strip, slot;
    const UnitID unit = r8::splitStripParam(d.id, strip, slot) ? UnitID(strip + 1) : kRootUnitId;
    parameters.addParameter(new RParam(d, unit));
  }
  return kResultOk;
}

tresult PLUGIN_API Controller::terminate() {
  if (uiBridge) { uiBridge->release(); uiBridge = nullptr; }
  return EditControllerEx1::terminate();
}

tresult PLUGIN_API Controller::setComponentState(IBStream* stream) {
  PluginState st;
  if (!st.read(stream)) return kResultFalse;
  for (const auto& p : st.params)
    if (r8::paramIndex(p.first) >= 0) setParamNormalized(ParamID(p.first), p.second);
  return kResultOk;
}

tresult PLUGIN_API Controller::setState(IBStream* s) {
  if (!s) return kResultFalse;
  uint32_t magic = 0, version = 0, colors = 0, sel = 0;
  double zoom = 1;
  int32 got = 0;
  if (s->read(&magic, 4, &got) != kResultOk || got != 4 || magic != kUiMagic) return kResultFalse;
  if (s->read(&version, 4, &got) != kResultOk || got != 4) return kResultFalse;
  if (s->read(&zoom, 8, &got) == kResultOk && got == 8 && zoom >= 0.5 && zoom <= 3) opts.zoom = zoom;
  s->read(&colors, 4, &got);                       // unused field (kept for older projects)
  if (s->read(&sel, 4, &got) == kResultOk && got == 4 && sel < uint32_t(r8::kMaxStrips)) opts.selected = int(sel);
  if (version >= 2) {
    uint32_t a[5];
    if (s->read(a, 20, &got) == kResultOk && got == 20) {
      opts.anMode = int(std::min<uint32_t>(a[0], 2)); opts.anSpeed = int(std::min<uint32_t>(a[1], 2));
      opts.anRange = int(std::min<uint32_t>(a[2], 2)); opts.anTilt = a[3] != 0; opts.anPeak = a[4] != 0;
    }
  }
  return kResultOk;
}

tresult PLUGIN_API Controller::getState(IBStream* s) {
  if (!s) return kResultFalse;
  uint32_t magic = kUiMagic, version = kUiVersion, colors = 0, sel = uint32_t(opts.selected);
  double zoom = opts.zoom;
  s->write(&magic, 4, nullptr);
  s->write(&version, 4, nullptr);
  s->write(&zoom, 8, nullptr);
  s->write(&colors, 4, nullptr);
  s->write(&sel, 4, nullptr);
  uint32_t a[5] = { uint32_t(opts.anMode), uint32_t(opts.anSpeed), uint32_t(opts.anRange), opts.anTilt ? 1u : 0u, opts.anPeak ? 1u : 0u };
  s->write(a, 20, nullptr);
  return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
  if (FIDStringsEqual(name, ViewType::kEditor)) return new Editor(this);
  return nullptr;
}

// the processor hands over the bridge once (same process, see bridge.h)
tresult PLUGIN_API Controller::notify(IMessage* message) {
  if (!message) return kInvalidArgument;
  if (FIDStringsEqual(message->getMessageID(), "bridge")) {
    int64 ptr = 0;
    if (message->getAttributes()->getInt("ptr", ptr) == kResultOk && ptr) {
      UiBridge* b = reinterpret_cast<UiBridge*>(ptr);
      b->addRef();
      if (uiBridge) uiBridge->release();
      uiBridge = b;
    }
    return kResultOk;
  }
  return EditControllerEx1::notify(message);
}

void Controller::edit(ParamID id, double value, int phase) {
  if (phase == 0) beginEdit(id);
  else if (phase == 1) {
    setParamNormalized(id, value);
    performEdit(id, value);
  } else endEdit(id);
}

} // namespace eightbus
