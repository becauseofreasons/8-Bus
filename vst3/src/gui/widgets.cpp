#include "widgets.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cgradient.h"
#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace eightbus {

namespace {
constexpr double kPi = 3.14159265358979323846;
inline double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
void fillCircle(CDrawContext* c, const CPoint& m, double r, const CColor& col) {
  c->setFillColor(col);
  c->drawEllipse(CRect(m.x - r, m.y - r, m.x + r, m.y + r), kDrawFilled);
}
bool fine(const Modifiers& m) { return m.has(ModifierKey::Shift) || m.has(ModifierKey::Control); }
}

// ------------------------------------------------------------------------------- ParamControl
void ParamControl::set(double v, bool single) {
  v = clamp01(v);
  val = v;
  invalid();
  if (single) { edit(id, v, 0); edit(id, v, 1); edit(id, v, 2); }
  else edit(id, v, 1);
}

// ------------------------------------------------------------------------------- Knob
CPoint Knob::center() const {
  const CRect& r = getViewSize();
  return CPoint(r.getCenter().x, r.top + radius + 5);
}

void Knob::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CPoint m = center();
  const theme::KnobColors col = theme::knobColors(role);
  // scale ticks
  c->setLineWidth(1.0);
  c->setFrameColor(theme::lineDim);
  for (int i = 0; i <= 10; i++) {
    const double a = (-150 + 30 * i) * kPi / 180;
    const double r0 = radius + 2.5, r1 = radius + (i == 0 || i == 5 || i == 10 ? 6.0 : 4.5);
    c->drawLine(CPoint(m.x + std::sin(a) * r0, m.y - std::cos(a) * r0), CPoint(m.x + std::sin(a) * r1, m.y - std::cos(a) * r1));
  }
  // knurled body
  PointList poly;
  const int teeth = 22;
  for (int i = 0; i < 2 * teeth; i++) {
    const double a = i * kPi / teeth;
    const double rr = (i & 1) ? radius - 1.4 : radius;
    poly.push_back(CPoint(m.x + std::sin(a) * rr, m.y - std::cos(a) * rr));
  }
  c->setFillColor(col.edge);
  c->drawPolygon(poly, kDrawFilled);
  fillCircle(c, m, radius - 2.0, col.body);
  fillCircle(c, m, radius * 0.62, col.cap);
  // pointer
  const double a = (-150 + 300 * val) * kPi / 180;
  c->setLineWidth(2.2);
  c->setLineStyle(CLineStyle(CLineStyle::kLineCapRound));
  c->setFrameColor(col.pointer);
  c->drawLine(CPoint(m.x + std::sin(a) * radius * 0.18, m.y - std::cos(a) * radius * 0.18),
              CPoint(m.x + std::sin(a) * (radius - 1.6), m.y - std::cos(a) * (radius - 1.6)));
  c->setLineStyle(kLineSolid);
  // range labels
  const double ly = m.y + radius * 0.62 + 4;
  theme::label(c, lo, CRect(m.x - radius - 15, ly, m.x - radius + 9, ly + 10), theme::dim, 7.5);
  theme::label(c, hi, CRect(m.x + radius - 9, ly, m.x + radius + 15, ly + 10), theme::dim, 7.5);
}

void Knob::onMouseDownEvent(MouseDownEvent& e) {
  if (!e.buttonState.isLeft()) return;
  if (e.clickCount == 2) {
    set(def, true);
    e.consumed = true;
    return;
  }
  drag = true;
  startY = e.mousePosition.y;
  startV = val;
  edit(id, val, 0);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Knob::onMouseMoveEvent(MouseMoveEvent& e) {
  if (!drag) return;
  const double range = fine(e.modifiers) ? 1200.0 : 220.0;
  set(startV + (startY - e.mousePosition.y) / range, false);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Knob::onMouseUpEvent(MouseUpEvent& e) {
  if (!drag) return;
  drag = false;
  edit(id, val, 2);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Knob::onMouseCancelEvent(MouseCancelEvent& e) {
  if (drag) { drag = false; edit(id, val, 2); if (onDrag) onDrag(this); }
  e.consumed = true;
}
void Knob::onMouseWheelEvent(MouseWheelEvent& e) {
  const double step = fine(e.modifiers) ? 0.002 : 0.012;
  set(val + (e.deltaY > 0 ? step : -step), true);
  e.consumed = true;
}

// ------------------------------------------------------------------------------- Fader
double Fader::top() const { return getViewSize().top + 10; }
double Fader::bottom() const { return getViewSize().bottom - 16; }

CRect Fader::cap() const {
  const CRect& r = getViewSize();
  const double x = r.left + 38, y = bottom() - val * (bottom() - top());
  return CRect(x - 11, y - 7, x + 11, y + 7);
}

void Fader::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CRect& r = getViewSize();
  const double x = r.left + 38;
  // scale: position -> mark
  static const std::pair<double, const char*> marks[] = { { 1.0, "+10" }, { 0.875, "5" }, { 0.75, "U" }, { 0.625, "5" },
    { 0.52, "10" }, { 0.37, "20" }, { 0.25, "30" }, { 0.15, "40" }, { 0.04, "60" }, { 0.0, "\xE2\x88\x9E" } };
  c->setLineWidth(1);
  for (const auto& mk : marks) {
    const double y = bottom() - mk.first * (bottom() - top());
    c->setFrameColor(mk.first == 0.75 ? theme::line : theme::lineDim);
    c->drawLine(CPoint(x - 13, y), CPoint(x - 6, y));
    c->drawLine(CPoint(x + 6, y), CPoint(x + 10, y));
    theme::label(c, mk.second, CRect(r.left + 1, y - 5, x - 14, y + 5), mk.first == 0.75 ? theme::text : theme::dim, 7.5, false, kRightText);
  }
  // slot
  c->setFillColor(theme::rgb(0x08, 0x08, 0x09));
  c->drawRect(CRect(x - 1.5, top() - 4, x + 1.5, bottom() + 4), kDrawFilled);
  // cap: dark with a white line (like the console)
  const CRect k = cap();
  c->setFillColor(theme::rgb(0x2c, 0x2e, 0x32));
  c->drawRect(k, kDrawFilled);
  c->setFillColor(theme::rgb(0x40, 0x43, 0x48));
  c->drawRect(CRect(k.left, k.top, k.right, k.top + 4), kDrawFilled);
  c->setFrameColor(theme::rgb(0xf0, 0xf0, 0xf0));
  c->setLineWidth(1.6);
  c->drawLine(CPoint(k.left + 2, k.getCenter().y), CPoint(k.right - 2, k.getCenter().y));
}

void Fader::onMouseDownEvent(MouseDownEvent& e) {
  if (!e.buttonState.isLeft()) return;
  if (e.clickCount == 2) { set(def, true); e.consumed = true; return; }
  drag = true;
  startY = e.mousePosition.y;
  startV = val;
  edit(id, val, 0);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Fader::onMouseMoveEvent(MouseMoveEvent& e) {
  if (!drag) return;
  const double range = (bottom() - top()) * (fine(e.modifiers) ? 5 : 1);
  set(startV + (startY - e.mousePosition.y) / range, false);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Fader::onMouseUpEvent(MouseUpEvent& e) {
  if (!drag) return;
  drag = false;
  edit(id, val, 2);
  if (onDrag) onDrag(this);
  e.consumed = true;
}
void Fader::onMouseCancelEvent(MouseCancelEvent& e) {
  if (drag) { drag = false; edit(id, val, 2); if (onDrag) onDrag(this); }
  e.consumed = true;
}
void Fader::onMouseWheelEvent(MouseWheelEvent& e) {
  const double step = fine(e.modifiers) ? 0.002 : 0.01;
  set(val + (e.deltaY > 0 ? step : -step), true);
  e.consumed = true;
}

// ------------------------------------------------------------------------------- Toggle
void Toggle::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CRect r = getViewSize();
  const bool on = val > 0.5;
  c->setFillColor(on ? theme::buttonOn : theme::buttonOff);
  c->drawRect(r, kDrawFilled);
  c->setFrameColor(on ? theme::text : theme::lineDim);
  c->setLineWidth(1);
  c->drawRect(r, kDrawStroked);
  theme::label(c, text, r, on ? theme::rgb(0x18, 0x18, 0x18) : theme::dim, 7.5, true);
}
void Toggle::onMouseDownEvent(MouseDownEvent& e) {
  if (!e.buttonState.isLeft()) return;
  set(val > 0.5 ? 0.0 : 1.0, true);
  e.consumed = true;
}

// ------------------------------------------------------------------------------- TextButton
void TextButton::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CRect r = getViewSize();
  c->setFillColor(active ? theme::buttonOn : theme::buttonOff);
  c->drawRect(r, kDrawFilled);
  theme::label(c, text, r, active ? theme::rgb(0x18, 0x18, 0x18) : theme::text, size, true);
}
void TextButton::onMouseDownEvent(MouseDownEvent& e) {
  if (e.buttonState.isRight() && onRightClick) { onRightClick(e.mousePosition); e.consumed = true; return; }
  if (!e.buttonState.isLeft()) return;
  if (onClick) onClick();
  e.consumed = true;
}

// ------------------------------------------------------------------------------- LedMeter
const float LedMeter::kSteps[13] = { -40, -30, -20, -10, -7, -4, -2, 0, 2, 4, 7, 10, 22 };

void LedMeter::setPeaks(float l, float r, int channels, double ms) {
  const float p[2] = { l, r };
  bool changed = channels != nch;
  nch = channels;
  for (int c = 0; c < 2; c++) {
    const double db = 20 * std::log10(std::max(1e-6f, p[c])) + 18.0;      // console scale: 0 = -18 dBFS
    const double fall = level[c] - 26.0 * ms / 1000.0;
    const double nv = std::max(db, fall);
    int before = 0, after = 0;
    for (float s : kSteps) { before += level[c] >= s; after += nv >= s; }
    level[c] = std::max(-100.0, nv);
    changed = changed || before != after;
  }
  if (changed) invalid();
}

void LedMeter::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CRect r = getViewSize();
  const double gap = 14, pitch = (r.getHeight() - gap) / 13.0, ledH = pitch * 0.72, ledW = 13;
  for (int i = 0; i < 13; i++) {
    const double y = r.bottom - (i + 1) * pitch - (i == 12 ? gap : 0) + (pitch - ledH) / 2;
    for (int ch = 0; ch < 2; ch++) {
      const double x = ch == 0 ? r.left + 1 : r.right - 1 - ledW;
      const bool on = level[nch == 1 ? 0 : ch] >= kSteps[i];
      const CColor col = i < 7 ? (on ? theme::ledGreenOn : theme::ledGreenOff)
                       : i < 12 ? (on ? theme::ledYellowOn : theme::ledYellowOff)
                                : (on ? theme::ledRedOn : theme::ledRedOff);
      c->setFillColor(col);
      c->drawRect(CRect(x, y, x + ledW, y + ledH), kDrawFilled);
      c->setFrameColor(theme::lineDim);
      c->setLineWidth(1);
      c->drawRect(CRect(x, y, x + ledW, y + ledH), kDrawStroked);
    }
    const int v = int(std::abs(kSteps[i]));
    const bool zero = kSteps[i] == 0;
    theme::label(c, std::to_string(v), CRect(r.left + ledW + 2, y, r.right - ledW - 2, y + ledH), zero ? theme::text : theme::dim, zero ? 9 : 8, zero);
  }
  theme::label(c, "+", CRect(r.left, r.top - 2, r.right, r.top + 8), theme::dim, 9);
  theme::label(c, "\xE2\x80\x93", CRect(r.left, r.bottom - 2, r.right, r.bottom + 10), theme::dim, 9);
}

// ------------------------------------------------------------------------------- OL LED
void OlLed::draw(CDrawContext* c) {
  const CRect r = getViewSize();
  c->setFillColor(lit ? theme::ledRedOn : theme::ledRedOff);
  c->drawRect(r, kDrawFilled);
  c->setFrameColor(theme::lineDim);
  c->setLineWidth(1);
  c->drawRect(r, kDrawStroked);
}

// ------------------------------------------------------------------------------- CurveView
double CurveView::freqAt(int i) { return 20.0 * std::pow(1000.0, double(i) / (Points - 1)); }

void CurveView::setCurves(const std::vector<double>& s, const std::vector<double>& ch, int si, int n) {
  sel = s; chain = ch; selIndex = si; strips = n;
  invalid();
}

void CurveView::setSpectrum(const std::vector<double>& in, const std::vector<double>& out, const std::vector<double>& peak,
                            int mode, double rangeDb, bool showPeak) {
  specIn = in; specOut = out; specPeak = peak; specMode = mode; specRange = rangeDb; specShowPeak = showPeak;
  invalid();
}

namespace {
// smooth path through points (Catmull-Rom as Bezier segments)
void splineTo(CGraphicsPath* path, const std::vector<CPoint>& pt, bool start) {
  if (pt.size() < 2) return;
  if (start) path->beginSubpath(pt[0]); else path->addLine(pt[0]);
  for (size_t i = 0; i + 1 < pt.size(); i++) {
    const CPoint& p0 = pt[i ? i - 1 : 0];
    const CPoint& p1 = pt[i];
    const CPoint& p2 = pt[i + 1];
    const CPoint& p3 = pt[i + 2 < pt.size() ? i + 2 : i + 1];
    path->addBezierCurve(CPoint(p1.x + (p2.x - p0.x) / 6, p1.y + (p2.y - p0.y) / 6),
                         CPoint(p2.x - (p3.x - p1.x) / 6, p2.y - (p3.y - p1.y) / 6), p2);
  }
}
}

void CurveView::draw(CDrawContext* c) {
  c->setDrawMode(kAntiAliasing | kNonIntegralMode);
  const CRect r = getViewSize();
  c->setFillColor(theme::rgb(0x0c, 0x0d, 0x0f));
  c->drawRect(r, kDrawFilled);
  const CRect p(r.left + 30, r.top + 22, r.right - 34, r.bottom - 18);
  const double range = 20;
  auto xOf = [&](double f) { return p.left + std::log(f / 20.0) / std::log(1000.0) * p.getWidth(); };
  auto yOf = [&](double db) { db = std::max(-range - 2, std::min(range + 2, db)); return p.getCenter().y - db / range * p.getHeight() / 2; };
  auto ySpec = [&](double db) { db = std::max(-specRange - 6, std::min(6.0, db)); return p.top - db / specRange * p.getHeight(); };

  // grid: very quiet
  c->setLineWidth(1);
  const double fl[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000,
                        6000, 7000, 8000, 9000, 10000, 20000 };
  for (double f : fl) {
    const bool major = f == 100 || f == 1000 || f == 10000;
    c->setFrameColor(major ? theme::rgb(0x2a, 0x2d, 0x32) : theme::rgb(0x18, 0x1a, 0x1d));
    c->drawLine(CPoint(xOf(f), p.top), CPoint(xOf(f), p.bottom));
  }
  for (int db = -18; db <= 18; db += 6) {
    c->setFrameColor(db == 0 ? theme::rgb(0x34, 0x37, 0x3c) : theme::rgb(0x18, 0x1a, 0x1d));
    c->drawLine(CPoint(p.left, yOf(db)), CPoint(p.right, yOf(db)));
    theme::label(c, (db > 0 ? "+" : "") + std::to_string(db), CRect(r.left, yOf(db) - 5, p.left - 5, yOf(db) + 5),
                 theme::rgb(0x8a, 0x8e, 0x94), 8, false, kRightText);
  }
  const double fLab[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
  const char* fn[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
  for (int i = 0; i < 8; i++)
    theme::label(c, fn[i], CRect(xOf(fLab[i]) - 16, p.bottom + 3, xOf(fLab[i]) + 16, p.bottom + 15), theme::rgb(0x8a, 0x8e, 0x94), 8.5);

  // live spectrum
  const bool spec = specMode != 0 && int(specOut.size()) == Points;
  if (spec) {
    const double step = specRange <= 60 ? 12 : 24;
    for (double db = 0; db >= -specRange; db -= step)
      theme::label(c, std::to_string(int(db)), CRect(p.right + 4, ySpec(db) - 5, r.right - 2, ySpec(db) + 5),
                   theme::rgb(0x5a, 0x7a, 0x66), 7.5, false, kLeftText);
    auto points = [&](const std::vector<double>& v) {
      std::vector<CPoint> pt;
      pt.reserve(size_t(Points));
      for (int i = 0; i < Points; i++) pt.push_back(CPoint(xOf(freqAt(i)), ySpec(v[size_t(i)])));
      return pt;
    };
    auto area = [&](const std::vector<double>& v, const CColor& top, const CColor& bottom, const CColor* outline) {
      const std::vector<CPoint> pt = points(v);
      auto fill = owned(c->createGraphicsPath());
      if (!fill) return;
      fill->beginSubpath(CPoint(pt.front().x, p.bottom));
      splineTo(fill, pt, false);
      fill->addLine(CPoint(pt.back().x, p.bottom));
      fill->closeSubpath();
      auto grad = owned(CGradient::create(0.0, 1.0, top, bottom));
      if (grad) c->fillLinearGradient(fill, *grad, CPoint(p.left, p.top), CPoint(p.left, p.bottom));
      if (outline) {
        auto line = owned(c->createGraphicsPath());
        splineTo(line, pt, true);
        c->setFrameColor(*outline);
        c->setLineWidth(1.2);
        c->drawGraphicsPath(line, CDrawContext::kPathStroked);
      }
    };
    c->saveGlobalState();
    c->setClipRect(p);
    if (specMode == 1 && int(specIn.size()) == Points)
    {
      const CColor inLine = theme::rgb(0x9a, 0xae, 0xd0, 150);
      area(specIn, theme::rgb(0x6e, 0x86, 0xb8, 130), theme::rgb(0x6e, 0x86, 0xb8, 0), &inLine);
    }
    const CColor outLine = theme::rgb(0x9c, 0xe6, 0xb0, 220);
    area(specOut, theme::rgb(0x5c, 0xc8, 0x84, 150), theme::rgb(0x2a, 0x6a, 0x46, 10), &outLine);
    if (specShowPeak && int(specPeak.size()) == Points) {
      auto line = owned(c->createGraphicsPath());
      splineTo(line, points(specPeak), true);
      c->setFrameColor(theme::rgb(0xc8, 0xf4, 0xd4, 140));
      c->setLineWidth(1.0);
      c->drawGraphicsPath(line, CDrawContext::kPathStroked);
    }
    c->restoreGlobalState();
  }

  // EQ curves in front: chain (blue) and the selected channel (orange, with a tinted area to 0 dB)
  auto curvePts = [&](const std::vector<double>& v) {
    std::vector<CPoint> pt;
    for (int i = 0; i < Points; i++) pt.push_back(CPoint(xOf(freqAt(i)), yOf(v[size_t(i)])));
    return pt;
  };
  c->saveGlobalState();
  c->setClipRect(CRect(p.left, p.top - 2, p.right, p.bottom + 2));
  if (int(sel.size()) == Points) {
    const std::vector<CPoint> pt = curvePts(sel);
    auto fill = owned(c->createGraphicsPath());
    if (fill) {
      fill->beginSubpath(CPoint(pt.front().x, yOf(0)));
      splineTo(fill, pt, false);
      fill->addLine(CPoint(pt.back().x, yOf(0)));
      fill->closeSubpath();
      c->setFillColor(theme::rgb(0xf0, 0xa8, 0x3c, 38));
      c->drawGraphicsPath(fill, CDrawContext::kPathFilled);
    }
  }
  auto stroke = [&](const std::vector<double>& v, const CColor& col, double w) {
    if (int(v.size()) != Points) return;
    auto path = owned(c->createGraphicsPath());
    if (!path) return;
    splineTo(path, curvePts(v), true);
    c->setFrameColor(col);
    c->setLineWidth(w);
    c->drawGraphicsPath(path, CDrawContext::kPathStroked);
  };
  if (strips > 1) stroke(chain, theme::chainCurve, 1.5);
  stroke(sel, theme::accent, 2.4);
  c->restoreGlobalState();

  // legend
  theme::label(c, "KANAL " + std::to_string(selIndex + 1), CRect(p.left, r.top + 4, p.left + 80, r.top + 17), theme::accent, 9, true, kLeftText);
  if (strips > 1)
    theme::label(c, "KETTE (" + std::to_string(strips) + ")", CRect(p.left + 90, r.top + 4, p.left + 200, r.top + 17), theme::chainCurve, 9, true, kLeftText);
  if (spec) {
    double x = p.right - 150;
    c->setFillColor(theme::rgb(0x5c, 0xc8, 0x84, 200));
    c->drawRect(CRect(x, r.top + 7, x + 10, r.top + 14), kDrawFilled);
    theme::label(c, "AUSGANG", CRect(x + 14, r.top + 4, x + 74, r.top + 17), theme::rgb(0x9c, 0xe6, 0xb0), 9, true, kLeftText);
    if (specMode == 1) {
      x += 78;
      c->setFillColor(theme::rgb(0x6e, 0x86, 0xb8, 230));
      c->drawRect(CRect(x, r.top + 7, x + 10, r.top + 14), kDrawFilled);
      theme::label(c, "EINGANG", CRect(x + 14, r.top + 4, x + 74, r.top + 17), theme::rgb(0xa8, 0xb4, 0xc8), 9, true, kLeftText);
    }
  }
}

} // namespace eightbus
