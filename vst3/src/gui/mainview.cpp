#include "mainview.h"
#include "params.h"
#include "sections.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include <cmath>
#include <cstdio>

using namespace VSTGUI;
using namespace r8;

namespace eightbus {

namespace {
// rows of a channel strip (knob centres)
constexpr double yHeader = 58, yTrim = 98, ySep1 = 124, yHmGain = 148, yHmFreq = 192, yBw = 236, ySep2 = 262,
                 yLmGain = 286, yLmFreq = 330, ySep3 = 356, yHi = 380, yLo = 424, ySep4 = 450, yButtons = 456,
                 yFader = 476, yBottom = 634;
constexpr double xCurve = 620, xCurveEnd = 1482, xOut = 1488;   // curve view 862 x 574 (3:2), full height
const char* kMinus = "\xE2\x88\x92";   // typographic minus
std::string minus(const char* s) { return std::string(kMinus) + s; }
CRect knobCell(double x0, double cy) { return CRect(x0, cy - 20, x0 + 60, cy + 24); }

// the setup window: frame, title and row labels (its controls are children)
class SetupPanel : public CViewContainer {
public:
  explicit SetupPanel(const CRect& r) : CViewContainer(r) { setBackgroundColor(theme::rgb(0x22, 0x24, 0x28)); }
  void drawBackgroundRect(CDrawContext* c, const CRect& r) override {
    CViewContainer::drawBackgroundRect(c, r);
    const CRect b(0, 0, getViewSize().getWidth(), getViewSize().getHeight());
    c->setDrawMode(kAntiAliasing | kNonIntegralMode);
    c->setFrameColor(theme::line);
    c->setLineWidth(1.2);
    c->drawRect(CRect(b.left + 0.5, b.top + 0.5, b.right - 0.5, b.bottom - 0.5), kDrawStroked);
    theme::label(c, "SETUP", CRect(10, 8, 200, 26), theme::text, 11, true, kLeftText);
    const std::pair<double, const char*> rows[] = { { 36, "OVERSAMPLING" }, { 62, "TOLERANZEN" },
      { 88, "ZOOM %" }, { 114, "PRESETS" }, { 140, "BYPASS" }, { 176, "NETZTEIL/MASSE" },
      { 252, "ANZEIGE" }, { 278, "TEMPO" }, { 304, "BEREICH" }, { 330, "NEIGUNG" }, { 356, "SPITZENWERT" } };
    c->setFrameColor(theme::lineDim);
    c->setLineWidth(1);
    c->drawLine(CPoint(10, 224), CPoint(b.right - 10, 224));
    theme::label(c, "ANALYZER", CRect(10, 228, 200, 246), theme::text, 10, true, kLeftText);
    for (const auto& l : rows) theme::label(c, l.second, CRect(10, l.first, 108, l.first + 18), theme::dim, 9, true, kLeftText);
    theme::label(c, "(Flattern)", CRect(10, 192, 108, 206), theme::lineDim, 8.5, false, kLeftText);
    char lat[64];
    std::snprintf(lat, sizeof lat, "Latenz %d Samples", Oversampler::Latency);
    theme::label(c, lat, CRect(10, b.bottom - 22, 300, b.bottom - 8), theme::lineDim, 8.5, false, kLeftText);
  }
};
}

MainView::MainView(EditorHost& h) : CViewContainer(CRect(0, 0, W, H)), host(h) {
  setBackgroundColor(theme::bg);
  auto edit = [this](uint32_t id, double v, int phase) { host.edit(id, v, phase); };

  for (int s = 0; s < kMaxStrips; s++) buildStrip(s);
  add = new TextButton(CRect(0, 0, 10, 10), "+", [this] { addStrip(); });
  add->setFontSize(20);
  addView(add);

  inMeter = new LedMeter(CRect(12, 70, 64, 466));
  outMeter = new LedMeter(CRect(xOut + 6, 70, xOut + 58, 466));
  addView(inMeter);
  addView(outMeter);
  inGain = new Knob(CRect(6, 500, 70, 548), kParamInGain, 0.5, edit, theme::KnobRole::Global, minus("24"), "+24", 17);
  outGain = new Knob(CRect(xOut, 500, xOut + 64, 548), kParamOutGain, 0.5, edit, theme::KnobRole::Global, minus("24"), "+24", 17);
  for (Knob* k : { inGain, outGain }) {
    k->onDrag = [this](ParamControl* c) { showBubble(c); };
    addView(k);
    controls.push_back(k);
  }

  curve = new CurveView(CRect(xCurve, 58, xCurveEnd, 632));
  addView(curve);

  // setup window: opens from the button bottom left, above the channel strips
  setupButton = new TextButton(CRect(10, 598, 68, 620), "SETUP", [this] { toggleSetup(); });
  setupButton->setFontSize(9);
  addView(setupButton);
  setup = new SetupPanel(CRect(76, 206, 430, 630));
  const double bx = 112;
  auto button = [&](double x, double y, double w, const char* t, std::function<void()> f) {
    auto* b = new TextButton(CRect(x, y, x + w, y + 18), t, std::move(f));
    b->setFontSize(9);
    setup->addView(b);
    return b;
  };
  button(318, 8, 24, "\xC3\x97", [this] { toggleSetup(); });
  for (int i = 0; i < 3; i++)
    os[i] = button(bx + 38 * i, 36, 34, i == 0 ? "2x" : i == 1 ? "4x" : "8x", [this, i] { setParam(kParamOversampling, i / 2.0); });
  for (int i = 0; i < 2; i++)
    tol[i] = button(bx + 54 * i, 62, 50, i ? "AN" : "AUS", [this, i] { setParam(kParamTolerance, i); });
  const double zooms[4] = { 0.75, 1.0, 1.25, 1.5 };
  const char* zt[4] = { "75", "100", "125", "150" };
  for (int i = 0; i < 4; i++)
    zoom[i] = button(bx + 38 * i, 88, 34, zt[i], [this, z = zooms[i]] { host.options().zoom = z; updateSetup(); host.setZoom(z); });
  presets = button(bx, 114, 104, "MEN\xC3\x9C \xE2\x96\xBE", [this] { presetMenu(CPoint(76 + 112, 206 + 134)); });
  bypass = button(bx, 140, 50, "AUS", [this] { setParam(kParamBypass, host.param(kParamBypass) > 0.5 ? 0.0 : 1.0); });
  // condition of supply and ground: how much the console flutters when it is driven hot
  supply = new Knob(CRect(bx - 4, 170, bx + 56, 214), kParamSupply, 0.5, edit, theme::KnobRole::Global, "aus", "\xC3\x97" "4", 13);
  supply->onDrag = [this](ParamControl* c) { showBubble(c); };
  setup->addView(supply);
  controls.push_back(supply);
  // analyzer settings (editor options, saved with the project)
  auto opt = [this](auto f) { return [this, f] { f(host.options()); updateSetup(); }; };
  const char* modes[3] = { "AUS", "VOR+NACH", "NACH" };
  for (int i = 0; i < 3; i++) anMode[i] = button(bx + 62 * i, 252, 58, modes[i], opt([i](UiOptions& o) { o.anMode = i; }));
  const char* speeds[3] = { "LANGSAM", "MITTEL", "SCHNELL" };
  for (int i = 0; i < 3; i++) anSpeed[i] = button(bx + 62 * i, 278, 58, speeds[i], opt([i](UiOptions& o) { o.anSpeed = i; }));
  const char* ranges[3] = { "60 dB", "90 dB", "120 dB" };
  for (int i = 0; i < 3; i++) anRange[i] = button(bx + 62 * i, 304, 58, ranges[i], opt([i](UiOptions& o) { o.anRange = i; }));
  anTilt[0] = button(bx, 330, 58, "0", opt([](UiOptions& o) { o.anTilt = false; }));
  anTilt[1] = button(bx + 62, 330, 96, "4,5 dB/OKT", opt([](UiOptions& o) { o.anTilt = true; }));
  anPeak[0] = button(bx, 356, 58, "AUS", opt([](UiOptions& o) { o.anPeak = false; }));
  anPeak[1] = button(bx + 62, 356, 58, "AN", opt([](UiOptions& o) { o.anPeak = true; }));
  setup->setVisible(false);
  addView(setup);

  layoutStrips(stripsOfNorm(host.param(kParamStrips)));
  updateSetup();
  tick(0);
}

void MainView::buildStrip(int s) {
  StripUi& u = strips[size_t(s)];
  const double x0 = stripX(s);
  auto edit = [this](uint32_t id, double v, int phase) { host.edit(id, v, phase); };
  auto knob = [&](int slot, double cy, theme::KnobRole role, std::string lo, std::string hi) {
    auto* k = new Knob(knobCell(x0, cy), stripParamId(s, slot), paramDefault(stripParamId(s, slot)), edit, role, std::move(lo), std::move(hi));
    k->onDrag = [this](ParamControl* c) { showBubble(c); };
    addView(k);
    controls.push_back(k);
    u.views.push_back(k);
    return k;
  };
  u.trim = knob(kTrim, yTrim, theme::KnobRole::Trim, minus("5"), "+40");
  u.hmGain = knob(kHmGain, yHmGain, theme::KnobRole::Level, minus("15"), "+15");
  u.hmFreq = knob(kHmFreq, yHmFreq, theme::KnobRole::Freq, "500", "18k");
  u.hmBw = knob(kHmBw, yBw, theme::KnobRole::Freq, "3", "1/12");
  u.lmGain = knob(kLmGain, yLmGain, theme::KnobRole::Level, minus("15"), "+15");
  u.lmFreq = knob(kLmFreq, yLmFreq, theme::KnobRole::Freq, "45", "3k");
  u.hi = knob(kHi, yHi, theme::KnobRole::Level, minus("15"), "+15");
  u.lo = knob(kLo, yLo, theme::KnobRole::Level, minus("15"), "+15");
  u.cut = new Toggle(CRect(x0 + 6, yButtons, x0 + 28, yButtons + 14), stripParamId(s, kLoCut), 0, edit, "CUT");
  u.eq = new Toggle(CRect(x0 + 32, yButtons, x0 + 54, yButtons + 14), stripParamId(s, kEqIn), 1, edit, "EQ");
  u.fader = new Fader(CRect(x0, yFader, x0 + 60, yBottom), stripParamId(s, kFader), kFaderUnity, edit);
  u.fader->onDrag = [this](ParamControl* c) { showBubble(c); };
  for (ParamControl* c : { (ParamControl*)u.cut, (ParamControl*)u.eq, (ParamControl*)u.fader }) {
    addView(c);
    controls.push_back(c);
    u.views.push_back(c);
  }
  u.ol = new OlLed(CRect(x0 + 47, yTrim - 22, x0 + 56, yTrim - 16));
  u.header = new TextButton(CRect(x0 + 4, yHeader, x0 + 42, yHeader + 14), std::to_string(s + 1), [this, s] { select(s); });
  u.header->setFontSize(9);
  u.header->onRightClick = [this, s](const CPoint& p) { stripMenu(s, p); };
  u.remove = new TextButton(CRect(x0 + 44, yHeader, x0 + 56, yHeader + 14), "\xC3\x97", [this, s] { removeStrip(s); });
  u.remove->setFontSize(10);
  for (CView* v : { (CView*)u.ol, (CView*)u.header, (CView*)u.remove }) { addView(v); u.views.push_back(v); }
}

void MainView::layoutStrips(int n) {
  if (n == shown) return;
  shown = n;
  for (int s = 0; s < kMaxStrips; s++) {
    for (CView* v : strips[size_t(s)].views) v->setVisible(s < n);
    strips[size_t(s)].remove->setVisible(s < n && n > 1);
  }
  if (n < kMaxStrips) {
    const double x0 = stripX(n);
    add->setViewSize(CRect(x0 + 10, yTrim - 18, x0 + 50, yTrim + 18));
    add->setMouseableArea(add->getViewSize());
    add->setVisible(true);
  } else {
    add->setVisible(false);
  }
  if (host.options().selected >= n) select(n - 1);
  invalid();
}

void MainView::drawBackgroundRect(CDrawContext* c, const CRect& r) {
  CViewContainer::drawBackgroundRect(c, r);
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  // title and frame print (white lines with the diagonal steps of the panel artwork)
  theme::label(c, "8-BUS", CRect(14, 6, 300, 30), theme::text, 20, true, kLeftText);
  c->setFrameColor(theme::line);
  c->setLineWidth(1.2);
  c->drawLine(CPoint(6, 34), CPoint(W - 6, 34));
  for (double x : { 72.0, 612.0, xOut }) {
    c->drawLine(CPoint(x, 46), CPoint(x + 12, 34));
    c->drawLine(CPoint(x, 46), CPoint(x, H - 6));
  }
  theme::label(c, "INPUT", CRect(6, 38, 72, 54), theme::text, 11, true);
  theme::label(c, "EQUALIZER", CRect(88, 38, 300, 54), theme::text, 11, true, kLeftText);
  theme::label(c, "EQ CURVE \xC2\xB7 ANALYZER", CRect(628, 38, 900, 54), theme::text, 11, true, kLeftText);
  theme::label(c, "OUTPUT", CRect(xOut, 38, W - 4, 54), theme::text, 11, true);
  theme::label(c, "IN", CRect(6, 548, 72, 562), theme::dim, 9, true);
  theme::label(c, "OUT", CRect(xOut, 548, W - 4, 562), theme::dim, 9, true);

  // legend column
  const std::pair<double, const char*> legend[] = { { yTrim, "TRIM" }, { yHmGain, "HI MID" }, { yHmFreq, "FREQ" }, { yBw, "BW oct" },
    { yLmGain, "LO MID" }, { yLmFreq, "FREQ" }, { yHi, "HI 12k" }, { yLo, "LO 80" }, { yButtons + 7, "CUT \xC2\xB7 EQ" }, { 555, "FADER" } };
  for (const auto& l : legend) theme::label(c, l.second, CRect(74, l.first - 6, 124, l.first + 6), theme::dim, 8.5, true, kRightText);
  c->setFrameColor(theme::lineDim);
  c->setLineWidth(1);
  c->drawLine(CPoint(124, yHmGain - 14), CPoint(124, yBw + 8));      // bracket of the hi mid section
  c->drawLine(CPoint(124, yLmGain - 14), CPoint(124, yLmFreq + 8));

  // channel slots
  const int n = shown < 1 ? 1 : shown;
  for (int s = 0; s < kMaxStrips; s++) {
    const double x0 = stripX(s);
    if (s >= n) {
      c->setFillColor(theme::slot);
      c->drawRect(CRect(x0 + 3, yHeader, x0 + 57, yBottom), kDrawFilled);
      continue;
    }
    c->setFillColor(theme::panel);
    c->drawRect(CRect(x0 + 1, yHeader - 2, x0 + 59, yBottom), kDrawFilled);
    if (s == host.options().selected) {
      c->setFillColor(theme::accent);
      c->drawRect(CRect(x0 + 4, yHeader + 15, x0 + 42, yHeader + 17), kDrawFilled);
    }
  }
  // section lines with the step (like the console print)
  c->setFrameColor(theme::line);
  c->setLineWidth(1.1);
  const double xe = stripX(n) - 4;
  for (double y : { ySep1, ySep2, ySep3, ySep4 }) {
    c->drawLine(CPoint(80, y - 4), CPoint(106, y - 4));
    c->drawLine(CPoint(106, y - 4), CPoint(114, y + 4));
    c->drawLine(CPoint(114, y + 4), CPoint(xe, y + 4));
  }
}

void MainView::drawRect(CDrawContext* c, const CRect& r) {
  CViewContainer::drawRect(c, r);
  if (!bubbleFor || !bubbleFor->dragging()) return;
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  c->setClipRect(getViewSize());
  c->setFillColor(theme::rgb(0xf2, 0xf2, 0xf0));
  c->drawRect(bubbleRect, kDrawFilled);
  theme::label(c, bubbleText(bubbleFor), bubbleRect, theme::rgb(0x14, 0x14, 0x14), 10, true);
}

std::string MainView::bubbleText(ParamControl* ctl) {
  const uint32_t id = ctl->paramId();
  const double v = ctl->value();
  int s, slot;
  if (splitStripParam(id, s, slot)) {
    auto p = [&](int sl) { return host.param(stripParamId(s, sl)); };
    char b[48];
    // gains at the actual frequency / bandwidth of this channel
    if (slot == kHmGain && std::fabs(v - 0.5) > 1e-4) { std::snprintf(b, sizeof b, "%+.1f dB", hiMidGainDb(v, p(kHmFreq), p(kHmBw))); return b; }
    if (slot == kLmGain && std::fabs(v - 0.5) > 1e-4) { std::snprintf(b, sizeof b, "%+.1f dB", loMidGainDb(v, p(kLmFreq))); return b; }
  }
  return paramText(id, v);
}

void MainView::showBubble(ParamControl* ctl) {
  if (bubbleFor) invalidRect(bubbleRect);
  bubbleFor = ctl->dragging() ? ctl : nullptr;
  if (!bubbleFor) return;
  CRect r = ctl->getViewSize();
  if (ctl->getParentView() == setup) r.offset(setup->getViewSize().left, setup->getViewSize().top);   // in the setup window
  double cx = r.getCenter().x, top = r.top - 4;
  if (auto* f = dynamic_cast<Fader*>(ctl)) top = f->cap().top - 6;
  bubbleRect = CRect(cx - 34, top - 18, cx + 34, top);
  if (bubbleRect.left < 2) bubbleRect.offset(2 - bubbleRect.left, 0);
  if (bubbleRect.right > W - 2) bubbleRect.offset(W - 2 - bubbleRect.right, 0);
  if (bubbleRect.top < 2) bubbleRect.offset(0, 2 - bubbleRect.top);
  invalidRect(bubbleRect);
}

void MainView::setParam(uint32_t id, double v) {
  host.edit(id, v, 0);
  host.edit(id, v, 1);
  host.edit(id, v, 2);
}

void MainView::select(int s) {
  host.options().selected = s;
  for (int i = 0; i < kMaxStrips; i++) strips[size_t(i)].header->setActive(false);
  updateCurves();
  invalid();
}

void MainView::copyStrip(int from, int to) {
  for (int slot = 0; slot < kSlots; slot++) setParam(stripParamId(to, slot), host.param(stripParamId(from, slot)));
}
void MainView::resetStrip(int s) {
  for (int slot = 0; slot < kSlots; slot++) setParam(stripParamId(s, slot), paramDefault(stripParamId(s, slot)));
}

void MainView::addStrip() {
  const int n = stripsOfNorm(host.param(kParamStrips));
  if (n >= kMaxStrips) return;
  resetStrip(n);
  setParam(kParamStrips, normOfStrips(n + 1));
  layoutStrips(n + 1);
  select(n);
}

void MainView::removeStrip(int s) {
  const int n = stripsOfNorm(host.param(kParamStrips));
  if (n <= 1 || s >= n) return;
  for (int j = s; j < n - 1; j++) copyStrip(j + 1, j);
  resetStrip(n - 1);
  setParam(kParamStrips, normOfStrips(n - 1));
  layoutStrips(n - 1);
  int sel = host.options().selected;
  if (sel > s || sel >= n - 1) sel = std::max(0, sel - 1);
  select(sel);
}

void MainView::stripMenu(int s, const CPoint& where) {
  CFrame* f = getFrame();
  if (!f) return;
  const int n = stripsOfNorm(host.param(kParamStrips));
  auto menu = makeOwned<COptionMenu>(CRect(where.x, where.y, where.x + 1, where.y + 1), nullptr, -1, nullptr, nullptr, COptionMenu::kPopupStyle);
  menu->addEntry("Kanal " + std::to_string(s + 1) + " zur\xC3\xBC" "cksetzen");
  menu->addEntry("Kanal duplizieren (hinten anf\xC3\xBCgen)", -1, n < kMaxStrips ? CMenuItem::kNoFlags : CMenuItem::kDisabled);
  menu->addEntry("Kanal entfernen", -1, n > 1 ? CMenuItem::kNoFlags : CMenuItem::kDisabled);
  CPoint p = where;
  localToFrame(p);
  menu->popup(f, p, [this, s, n](COptionMenu* m) {
    switch (m->getLastResult()) {
      case 0: resetStrip(s); break;
      case 1:
        if (n < kMaxStrips) { copyStrip(s, n); setParam(kParamStrips, normOfStrips(n + 1)); layoutStrips(n + 1); select(n); }
        break;
      case 2: removeStrip(s); break;
      default: break;
    }
  });
}

void MainView::presetMenu(const CPoint& where) {
  CFrame* f = getFrame();
  if (!f) return;
  auto menu = makeOwned<COptionMenu>(CRect(where.x, where.y, where.x + 1, where.y + 1), nullptr, -1, nullptr, nullptr, COptionMenu::kPopupStyle);
  menu->addEntry("Init: ein Kanal, alles neutral");
  menu->addSeparator();
  menu->addEntry("Werks-Presets folgen", -1, CMenuItem::kDisabled);
  CPoint p = where;
  localToFrame(p);
  menu->popup(f, p, [this](COptionMenu* m) {
    if (m->getLastResult() != 0) return;
    for (const ParamDef& d : paramDefs())
      if (d.id != kParamOversampling && d.id != kParamTolerance) setParam(d.id, d.def);
    layoutStrips(1);
    select(0);
  });
}

void MainView::toggleSetup() {
  setup->setVisible(!setup->isVisible());
  setupButton->setActive(setup->isVisible());
  invalid();
}

void MainView::updateSetup() {
  const int osIndex = discrete(host.param(kParamOversampling), 2);
  for (int i = 0; i < 3; i++) os[i]->setActive(i == osIndex);
  const bool t = host.param(kParamTolerance) > 0.5;
  tol[0]->setActive(!t); tol[1]->setActive(t);
  const double zooms[4] = { 0.75, 1.0, 1.25, 1.5 };
  for (int i = 0; i < 4; i++) zoom[i]->setActive(std::fabs(host.options().zoom - zooms[i]) < 0.01);
  const UiOptions& o = host.options();
  for (int i = 0; i < 3; i++) { anMode[i]->setActive(o.anMode == i); anSpeed[i]->setActive(o.anSpeed == i); anRange[i]->setActive(o.anRange == i); }
  anTilt[0]->setActive(!o.anTilt); anTilt[1]->setActive(o.anTilt);
  anPeak[0]->setActive(!o.anPeak); anPeak[1]->setActive(o.anPeak);
  const bool bp = host.param(kParamBypass) > 0.5;
  bypass->setActive(bp);
  bypass->setText(bp ? "AN" : "AUS");
}

void MainView::updateCurves() {
  const int n = stripsOfNorm(host.param(kParamStrips));
  std::vector<double> chain(CurveView::Points, 0.0), sel(CurveView::Points, 0.0);
  const int selected = std::min(host.options().selected, n - 1);
  const Tolerance nominal;
  for (int s = 0; s < n; s++) {
    auto p = [&](int slot) { return host.param(stripParamId(s, slot)); };
    CurveCache& cc = cache[size_t(s)];
    const double keys[4][4] = { { 0, 0, 0, 0 }, { p(kLmGain), p(kLmFreq), 0, 0 }, { p(kHmGain), p(kHmFreq), p(kHmBw), 0 }, { p(kHi), p(kLo), 0, 0 } };
    for (int sec = 0; sec < 4; sec++) {
      bool same = cc.valid[sec];
      for (int k = 0; k < 4 && same; k++) same = cc.key[sec][k] == keys[sec][k];
      if (same) continue;
      Netlist net;
      if (sec == 0) buildLoCut(net, nominal);
      else if (sec == 1) buildLoMid(net, keys[1][0], keys[1][1], nominal);
      else if (sec == 2) buildHiMid(net, keys[2][0], keys[2][1], keys[2][2], nominal);
      else buildHiLo(net, keys[3][0], keys[3][1], nominal);
      cc.db[sec].resize(CurveView::Points);
      for (int i = 0; i < CurveView::Points; i++)
        cc.db[sec][size_t(i)] = 20 * std::log10(std::abs(response(net, CurveView::freqAt(i))) + 1e-9);
      for (int k = 0; k < 4; k++) cc.key[sec][k] = keys[sec][k];
      cc.valid[sec] = true;
    }
    const bool cut = p(kLoCut) > 0.5, eq = p(kEqIn) > 0.5;
    for (int i = 0; i < CurveView::Points; i++) {
      double v = 0;
      if (cut) v += cc.db[0][size_t(i)];
      if (eq) v += cc.db[1][size_t(i)] + cc.db[2][size_t(i)] + cc.db[3][size_t(i)];
      chain[size_t(i)] += v;
      if (s == selected) sel[size_t(i)] = v;
    }
  }
  curve->setCurves(sel, chain, selected, n);
}

void MainView::tick(double ms) {
  const int n = stripsOfNorm(host.param(kParamStrips));
  layoutStrips(n);
  bool paramsChanged = false;
  for (ParamControl* c : controls) {
    if (c->dragging()) { paramsChanged = true; continue; }
    const double v = host.param(c->paramId());
    if (v != c->value()) { c->setValue(v); paramsChanged = true; }
  }
  r8::Meters m;
  if (host.meters(m)) {
    const int ch = m.out[1] > 0 || m.in[1] > 0 ? 2 : 1;
    inMeter->setPeaks(m.in[0], m.in[1], ch, ms);
    outMeter->setPeaks(m.out[0], m.out[1], ch, ms);
    for (int s = 0; s < kMaxStrips; s++) {
      StripUi& u = strips[size_t(s)];
      if (m.ol[s] > Strip::OlThreshold) u.olHold = 250;
    }
  } else {
    inMeter->setPeaks(0, 0, 2, ms);
    outMeter->setPeaks(0, 0, 2, ms);
  }
  for (int s = 0; s < kMaxStrips; s++) {
    StripUi& u = strips[size_t(s)];
    u.olHold = std::max(0.0, u.olHold - ms);
    u.ol->set(u.olHold > 0);
  }
  updateSetup();
  if (paramsChanged || ms == 0) updateCurves();
  // live spectrum
  audioIn.resize(8192);
  audioOut.resize(8192);
  int got;
  while ((got = host.audio(audioIn.data(), audioOut.data(), 8192)) > 0) analyzer.push(audioIn.data(), audioOut.data(), got);
  {
    const UiOptions& o = host.options();
    AnalyzerSettings as;
    as.mode = o.anMode; as.speed = o.anSpeed; as.range = o.anRange; as.tilt = o.anTilt; as.peak = o.anPeak;
    if (analyzer.update(host.sampleRate(), ms > 0 ? ms : 16, &CurveView::freqAt, as) || as.mode != lastAnMode || as.range != lastAnRange || as.peak != lastAnPeak)
      curve->setSpectrum(analyzer.input(), analyzer.output(), analyzer.peak(), as.mode, as.rangeDb(), as.peak);
    lastAnMode = as.mode; lastAnRange = as.range; lastAnPeak = as.peak;
  }
}

} // namespace eightbus
