#include "editor.h"
#include "controller.h"
#include "linux_runloop.h"
#include "vstgui/lib/cframe.h"
#include <cstring>

using namespace VSTGUI;
using namespace Steinberg;

namespace eightbus {

static ViewRect zoomed(double z) { return ViewRect(0, 0, int32(MainView::W * z + 0.5), int32(MainView::H * z + 0.5)); }

Editor::Editor(Controller* c) : VSTGUIEditor(c), ctl(c) {
  ViewRect r = zoomed(ctl->options().zoom);
  setRect(r);
}

bool PLUGIN_API Editor::open(void* parent, const PlatformType& platformType) {
  if (frame) return false;
  frame = new CFrame(CRect(0, 0, MainView::W, MainView::H), this);
  frame->setTransparency(false);
  main = new MainView(*this);
  frame->addView(main);
  bool ok;
#if SMTG_OS_LINUX
  VSTGUI::X11::FrameConfig config;
  config.runLoop = makeOwned<HostRunLoop>(plugFrame);
  ok = frame->open(parent, platformType, &config);
#else
  ok = frame->open(parent, platformType);
#endif
  if (!ok) { frame->forget(); frame = nullptr; main = nullptr; return false; }
  const double z = ctl->options().zoom;
  if (z != 1.0) frame->setZoom(z);
  timer = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) { if (main) main->tick(16); }, 16, true);     // ~60 frames per second (analyzer)
  return true;
}

void PLUGIN_API Editor::close() {
  if (timer) { timer->stop(); timer = nullptr; }
  if (frame) {
    frame->close();          // releases the views
    frame = nullptr;
  }
  main = nullptr;
}

tresult PLUGIN_API Editor::isPlatformTypeSupported(FIDString type) {
#if SMTG_OS_LINUX
  if (strcmp(type, kPlatformTypeX11EmbedWindowID) != 0) return kInvalidArgument;     // X11 only (also under Xwayland)
#endif
  return VSTGUIEditor::isPlatformTypeSupported(type);
}

double Editor::param(uint32_t id) { return ctl->getParamNormalized(Vst::ParamID(id)); }
void Editor::edit(uint32_t id, double value, int phase) { ctl->edit(Vst::ParamID(id), value, phase); }
bool Editor::meters(r8::Meters& m) {
  UiBridge* b = ctl->bridge();
  return b && b->read(m);
}
UiOptions& Editor::options() { return ctl->options(); }
int Editor::audio(float* in, float* out, int max) {
  UiBridge* b = ctl->bridge();
  return b ? b->readAudio(in, out, max) : 0;
}
double Editor::sampleRate() {
  UiBridge* b = ctl->bridge();
  return b ? b->sampleRate() : 48000;
}

void Editor::setZoom(double z) {
  if (!frame) return;
  const double old = frame->getZoom();
  ViewRect r = zoomed(z);
  frame->setZoom(z);
  // ask the host first (it calls onSize); our own rect must still be the old size here
  if (!plugFrame) { setRect(r); return; }
  if (plugFrame->resizeView(this, &r) != kResultTrue) {
    frame->setZoom(old);                 // host refused: stay as we were
    ctl->options().zoom = old;
  }
}

} // namespace eightbus
