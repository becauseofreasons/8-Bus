// Renders the editor without a window into PNG files, with parameter values and meters from a stub host.
//   r8_uishot <output folder> [scale]
#include "../src/gui/mainview.h"
#include "params.h"
#include "engine.h"
#include <random>
#include <vector>
#include "vstgui/lib/vstguiinit.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/iplatformbitmap.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
#include <fstream>
#include <map>
#include <string>

using namespace VSTGUI;
using namespace eightbus;

struct StubHost : EditorHost {
  std::map<uint32_t, double> values;
  UiOptions opts;
  r8::Meters m;
  bool haveMeters = false;
  StubHost() { for (const auto& d : r8::paramDefs()) values[d.id] = d.def; }
  double param(uint32_t id) override { return values[id]; }
  void edit(uint32_t id, double v, int phase) override { if (phase == 1) values[id] = v; }
  bool meters(r8::Meters& out) override { out = m; return haveMeters; }
  UiOptions& options() override { return opts; }
  void setZoom(double) override {}
  // a 909-like kick for the analyzer: input clean, output driven
  int sent = 0;
  bool give = false;
  // test tone mode: pink noise + 100 Hz sine through the real engine with the current values
  r8::Engine* engine = nullptr;
  std::mt19937 rng{ 7 };
  double pk[7] = {};
  float sampleOf() {
    // pink noise (Paul Kellet) at about -30 dBFS rms, sine 100 Hz at -12 dBFS peak
    const double w = std::uniform_real_distribution<double>(-1, 1)(rng);
    pk[0] = 0.99886 * pk[0] + w * 0.0555179; pk[1] = 0.99332 * pk[1] + w * 0.0750759;
    pk[2] = 0.96900 * pk[2] + w * 0.1538520; pk[3] = 0.86650 * pk[3] + w * 0.3104856;
    pk[4] = 0.55000 * pk[4] + w * 0.5329522; pk[5] = -0.7616 * pk[5] - w * 0.0168980;
    const double pink = (pk[0] + pk[1] + pk[2] + pk[3] + pk[4] + pk[5] + pk[6] + w * 0.5362) * 0.11;
    pk[6] = w * 0.115926;
    return float(0.12 * pink + 0.25 * std::sin(2 * 3.14159265358979 * 100.0 * sent++ / 48000.0));
  }
  int audio(float* in, float* out, int max) override {
    if (!give) return 0;
    give = false;                      // one frame (16 ms) per editor tick
    if (engine) {
      const int n = std::min(max, 768);
      for (int i = 0; i < n; i++) in[i] = sampleOf();
      const float* ip = in;
      float* op = out;
      engine->process(&ip, &op, 1, n);
      engine->takeMeters(m);
      return n;
    }
    const int n = std::min(max, 768);
    for (int i = 0; i < n; i++) {
      const double t = std::fmod((sent + i) / 48000.0, 0.4);
      const double ph = 2 * 3.14159265358979 * (54.3 * t + 200 * 0.03 * (1 - std::exp(-t / 0.03)));
      const double k = std::sin(ph) * std::exp(-t / 0.08) + 0.2 * std::exp(-t / 0.001);
      in[i] = float(0.5 * k);
      out[i] = float(std::tanh(4 * k) * 0.9);
    }
    sent += n;
    return n;
  }
};

static double gScale = 1.0;
static void render(MainView& v, const std::string& path) {
  auto ctx = COffscreenContext::create(CPoint(MainView::W, MainView::H), gScale);
  if (!ctx) { std::printf("kein Offscreen-Zeichnen auf diesem System (%s)\n", path.c_str()); return; }
  ctx->beginDraw();
  {
    // the Cairo backend (Linux) only enlarges the bitmap: scale the drawing ourselves
    const double s = std::abs(ctx->getScaleFactor() - gScale) < 1e-6 && gScale != 1.0 &&
                     ctx->getBitmap()->getPlatformBitmap()->getSize().x > MainView::W * 1.01 ? gScale : 1.0;
    CDrawContext::Transform t(*ctx, CGraphicsTransform().scale(s, s));
    v.drawRect(ctx, CRect(0, 0, MainView::W, MainView::H));
  }
  ctx->endDraw();
  auto png = getPlatformFactory().createBitmapMemoryPNGRepresentation(ctx->getBitmap()->getPlatformBitmap());
  std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
  std::printf("geschrieben: %s (%zu Byte)\n", path.c_str(), png.size());
}

int main(int argc, char** argv) {
  std::string out = argc > 1 ? argv[1] : ".";
  if (argc > 2) gScale = std::atof(argv[2]);
#ifdef _WIN32
  VSTGUI::init(GetModuleHandle(nullptr));
#else
  VSTGUI::init(nullptr);
#endif
  {
    StubHost host;
    auto* view = new MainView(host);
    view->tick(0);
    render(*view, out + "/ui_default.png");

    // four channels set up for a 909 kick, meters busy, channel 2 selected
    host.values[r8::kParamStrips] = r8::normOfStrips(4);
    for (int s = 0; s < 4; s++) {
      host.values[r8::stripParamId(s, r8::kLo)] = 0.85 - 0.05 * s;
      host.values[r8::stripParamId(s, r8::kHmGain)] = 0.7;
      host.values[r8::stripParamId(s, r8::kHmFreq)] = 0.3 + 0.1 * s;
      host.values[r8::stripParamId(s, r8::kHmBw)] = 0.6;
      host.values[r8::stripParamId(s, r8::kLmGain)] = 0.35;
      host.values[r8::stripParamId(s, r8::kFader)] = 0.85;
      host.values[r8::stripParamId(s, r8::kTrim)] = 0.3;
    }
    host.values[r8::stripParamId(1, r8::kLoCut)] = 1;
    host.values[r8::stripParamId(3, r8::kEqIn)] = 0;
    host.opts.selected = 1;
    host.m.in[0] = 0.9f; host.m.in[1] = 0.85f; host.m.out[0] = 1.2f; host.m.out[1] = 1.1f;
    host.m.ol[0] = 5; host.m.ol[2] = 4;
    host.m.strips = 4;
    host.haveMeters = true;
    for (int i = 0; i < 25; i++) { host.give = true; view->tick(16); }   // the kick just after a hit
    render(*view, out + "/ui_chain.png");
    view->openSetup(true);
    view->tick(30);
    render(*view, out + "/ui_setup.png");
    view->openSetup(false);

    // test tone through the real engine (same channel settings), analyzer and meters live
    r8::Engine eng;
    eng.prepare(48000, 768);
    for (const auto& kv : host.values) eng.setParam(kv.first, kv.second);
    host.engine = &eng;
    for (int i = 0; i < 150; i++) { host.give = true; view->tick(16); }   // 2.4 s: everything settled
    render(*view, out + "/ui_testton.png");
    host.engine = nullptr;
    view->forget();
  }
  VSTGUI::exit();
  return 0;
}
