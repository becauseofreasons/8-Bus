#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "bridge.h"
#include "../gui/uioptions.h"

namespace eightbus {

class Controller : public Steinberg::Vst::EditControllerEx1 {
public:
  static Steinberg::FUnknown* createInstance(void*) { return (Steinberg::Vst::IEditController*)new Controller; }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

  // for the editor
  UiBridge* bridge() const { return uiBridge; }
  UiOptions& options() { return opts; }
  void edit(Steinberg::Vst::ParamID id, double value, int phase);    // 0 begin, 1 perform, 2 end

  OBJ_METHODS(Controller, EditControllerEx1)
  REFCOUNT_METHODS(EditControllerEx1)

private:
  UiBridge* uiBridge = nullptr;
  UiOptions opts;
};

} // namespace eightbus
