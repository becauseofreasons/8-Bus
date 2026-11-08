#pragma once
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cvstguitimer.h"
#include "../gui/mainview.h"

namespace eightbus {

class Controller;

class Editor : public Steinberg::Vst::VSTGUIEditor, public EditorHost {
public:
  explicit Editor(Controller* c);
  bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
  void PLUGIN_API close() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) SMTG_OVERRIDE;

  // EditorHost
  double param(uint32_t id) override;
  void edit(uint32_t id, double value, int phase) override;
  bool meters(r8::Meters& m) override;
  UiOptions& options() override;
  void setZoom(double zoom) override;
  int audio(float* in, float* out, int max) override;
  double sampleRate() override;

private:
  Controller* ctl;
  MainView* main = nullptr;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
};

} // namespace eightbus
