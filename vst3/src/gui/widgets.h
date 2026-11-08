// Controls of the editor, all drawn in code: knob, fader, buttons, LED meter, OL LED, EQ curve.
#pragma once
#include "theme.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/events.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace eightbus {

// phase 0 begin, 1 perform, 2 end
using EditFn = std::function<void(uint32_t id, double value, int phase)>;

class ParamControl : public VSTGUI::CView {
public:
  ParamControl(const VSTGUI::CRect& r, uint32_t id, double def, EditFn edit) : CView(r), id(id), def(def), edit(std::move(edit)) {}
  uint32_t paramId() const { return id; }
  void setParamId(uint32_t i) { id = i; }
  double value() const { return val; }
  void setValue(double v) { if (v != val) { val = v; invalid(); } }
  bool dragging() const { return drag; }
  std::function<void(ParamControl*)> onDrag;      // called while dragging (for the value bubble)
protected:
  uint32_t id;
  double def, val = 0;
  EditFn edit;
  bool drag = false;
  void set(double v, bool single);                 // clamps, sends to the host
};

class Knob : public ParamControl {
public:
  Knob(const VSTGUI::CRect& r, uint32_t id, double def, EditFn edit, theme::KnobRole role, std::string lo, std::string hi,
       double radius = 15)
    : ParamControl(r, id, def, std::move(edit)), role(role), lo(std::move(lo)), hi(std::move(hi)), radius(radius) {}
  void setLabels(std::string l, std::string h) { lo = std::move(l); hi = std::move(h); invalid(); }
  void draw(VSTGUI::CDrawContext* c) override;
  void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override;
  void onMouseMoveEvent(VSTGUI::MouseMoveEvent& e) override;
  void onMouseUpEvent(VSTGUI::MouseUpEvent& e) override;
  void onMouseCancelEvent(VSTGUI::MouseCancelEvent& e) override;
  void onMouseWheelEvent(VSTGUI::MouseWheelEvent& e) override;
  VSTGUI::CPoint center() const;
private:
  theme::KnobRole role;
  std::string lo, hi;
  double radius;
  double startY = 0, startV = 0;
};

class Fader : public ParamControl {
public:
  Fader(const VSTGUI::CRect& r, uint32_t id, double def, EditFn edit) : ParamControl(r, id, def, std::move(edit)) {}
  void draw(VSTGUI::CDrawContext* c) override;
  void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override;
  void onMouseMoveEvent(VSTGUI::MouseMoveEvent& e) override;
  void onMouseUpEvent(VSTGUI::MouseUpEvent& e) override;
  void onMouseCancelEvent(VSTGUI::MouseCancelEvent& e) override;
  void onMouseWheelEvent(VSTGUI::MouseWheelEvent& e) override;
  VSTGUI::CRect cap() const;
private:
  double startY = 0, startV = 0;
  double top() const;
  double bottom() const;
};

// square console button with a lit face when on (parameter 0/1)
class Toggle : public ParamControl {
public:
  Toggle(const VSTGUI::CRect& r, uint32_t id, double def, EditFn edit, std::string text)
    : ParamControl(r, id, def, std::move(edit)), text(std::move(text)) {}
  void draw(VSTGUI::CDrawContext* c) override;
  void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override;
private:
  std::string text;
};

// flat text button (setup panel, menus); "active" draws it lit
class TextButton : public VSTGUI::CView {
public:
  TextButton(const VSTGUI::CRect& r, std::string text, std::function<void()> onClick)
    : CView(r), text(std::move(text)), onClick(std::move(onClick)) {}
  void setActive(bool a) { if (a != active) { active = a; invalid(); } }
  void setText(const std::string& t) { if (t != text) { text = t; invalid(); } }
  void setFontSize(double s) { size = s; }
  void draw(VSTGUI::CDrawContext* c) override;
  void onMouseDownEvent(VSTGUI::MouseDownEvent& e) override;
  std::function<void(const VSTGUI::CPoint&)> onRightClick;
private:
  std::string text;
  std::function<void()> onClick;
  bool active = false;
  double size = 10;
};

// LED meter L/R with the console scale (-40 .. +22, 0 = 0 dBu = -18 dBFS)
class LedMeter : public VSTGUI::CView {
public:
  explicit LedMeter(const VSTGUI::CRect& r) : CView(r) {}
  void setPeaks(float l, float r, int channels, double ms);     // linear (1.0 = 0 dBFS)
  void draw(VSTGUI::CDrawContext* c) override;
  static const float kSteps[13];
private:
  double level[2] = { -100, -100 };
  int nch = 2;
};

// red OL LED of a channel
class OlLed : public VSTGUI::CView {
public:
  explicit OlLed(const VSTGUI::CRect& r) : CView(r) {}
  void set(bool on) { if (on != lit) { lit = on; invalid(); } }
  void draw(VSTGUI::CDrawContext* c) override;
private:
  bool lit = false;
};

// EQ curve: the selected channel and the whole chain (dB over log frequency), behind it the live spectrum
// of input and output (filled with a gradient, smooth outline, peak line), scale dBFS on the right
class CurveView : public VSTGUI::CView {
public:
  static constexpr int Points = 220;
  explicit CurveView(const VSTGUI::CRect& r) : CView(r) {}
  static double freqAt(int i);
  void setCurves(const std::vector<double>& selected, const std::vector<double>& chain, int sel, int strips);
  // mode 0 off, 1 input + output, 2 output only; rangeDb: shown spectrum range below 0 dBFS
  void setSpectrum(const std::vector<double>& in, const std::vector<double>& out, const std::vector<double>& peak,
                   int mode, double rangeDb, bool showPeak);
  void draw(VSTGUI::CDrawContext* c) override;
private:
  std::vector<double> sel, chain, specIn, specOut, specPeak;
  int selIndex = 0, strips = 1, specMode = 1;
  double specRange = 90;
  bool specShowPeak = true;
};

} // namespace eightbus
