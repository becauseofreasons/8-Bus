// The whole editor content. Talks to the outside only through EditorHost, so it also runs
// without the plugin (test/uishot.cpp renders it into a PNG).
//
// Layout like the panel artwork: INPUT meter | channel strips side by side (1..8, "+" adds one) |
// EQ curve and setup | OUTPUT meter.
#pragma once
#include "engine.h"
#include "analyzer.h"
#include "uioptions.h"
#include "widgets.h"
#include "vstgui/lib/cviewcontainer.h"
#include <array>
#include <vector>

namespace eightbus {

struct EditorHost {
  virtual ~EditorHost() = default;
  virtual double param(uint32_t id) = 0;                       // normalized
  virtual void edit(uint32_t id, double value, int phase) = 0; // phase 0 begin, 1 perform, 2 end
  virtual bool meters(r8::Meters& m) = 0;                      // new meter data since the last call
  virtual UiOptions& options() = 0;
  virtual void setZoom(double zoom) = 0;
  virtual int audio(float* in, float* out, int max) { return 0; }   // analyzer samples since the last call
  virtual double sampleRate() { return 48000; }
};

class MainView : public VSTGUI::CViewContainer {
public:
  static constexpr double W = 1560, H = 640;
  explicit MainView(EditorHost& host);
  void tick(double ms);                                 // timer, ~30 ms
  void drawBackgroundRect(VSTGUI::CDrawContext* c, const VSTGUI::CRect& r) override;
  void drawRect(VSTGUI::CDrawContext* c, const VSTGUI::CRect& r) override;
  void openSetup(bool on) { if (on != setup->isVisible()) toggleSetup(); }

private:
  EditorHost& host;
  struct StripUi {
    Knob* trim = nullptr; Knob* hmGain = nullptr; Knob* hmFreq = nullptr; Knob* hmBw = nullptr;
    Knob* lmGain = nullptr; Knob* lmFreq = nullptr; Knob* hi = nullptr; Knob* lo = nullptr;
    Toggle* cut = nullptr; Toggle* eq = nullptr; Fader* fader = nullptr;
    OlLed* ol = nullptr; TextButton* header = nullptr; TextButton* remove = nullptr;
    double olHold = 0;
    std::vector<VSTGUI::CView*> views;
  };
  std::array<StripUi, r8::kMaxStrips> strips;
  TextButton* add = nullptr;
  Knob* inGain = nullptr; Knob* outGain = nullptr; Knob* supply = nullptr;
  LedMeter* inMeter = nullptr; LedMeter* outMeter = nullptr;
  CurveView* curve = nullptr;
  std::vector<ParamControl*> controls;
  TextButton* os[3] = {}; TextButton* tol[2] = {}; TextButton* zoom[4] = {};
  TextButton* presets = nullptr; TextButton* bypass = nullptr;
  TextButton* anMode[3] = {}; TextButton* anSpeed[3] = {}; TextButton* anRange[3] = {}; TextButton* anTilt[2] = {}; TextButton* anPeak[2] = {};
  VSTGUI::CViewContainer* setup = nullptr;       // setup window (opens from the button bottom left)
  TextButton* setupButton = nullptr;
  Analyzer analyzer{ CurveView::Points };
  std::vector<float> audioIn, audioOut;
  void toggleSetup();
  int lastAnMode = -1, lastAnRange = -1;
  bool lastAnPeak = true;
  int shown = -1;                               // number of visible channels
  ParamControl* bubbleFor = nullptr;
  VSTGUI::CRect bubbleRect;

  // EQ curve cache: per channel the dB of each section, recomputed when its knobs move
  struct CurveCache {
    double key[4][4];
    bool valid[4] = {};
    std::vector<double> db[4];
  };
  std::array<CurveCache, r8::kMaxStrips> cache;

  static double stripX(int s) { return 128 + 60.0 * s; }
  void buildStrip(int s);
  void layoutStrips(int n);
  void updateSetup();
  void updateCurves();
  void setParam(uint32_t id, double v);
  void addStrip();
  void removeStrip(int s);
  void resetStrip(int s);
  void copyStrip(int from, int to);
  void stripMenu(int s, const VSTGUI::CPoint& where);
  void presetMenu(const VSTGUI::CPoint& where);
  void select(int s);
  void showBubble(ParamControl* c);
  std::string bubbleText(ParamControl* c);
};

} // namespace eightbus
