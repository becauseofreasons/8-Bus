// Colours and fonts: black / dark grey panel, light print
#pragma once
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cdrawcontext.h"
#include <string>

namespace eightbus {
namespace theme {

using VSTGUI::CColor;
inline CColor rgb(int r, int g, int b, int a = 255) { return CColor(uint8_t(r), uint8_t(g), uint8_t(b), uint8_t(a)); }

const CColor bg = rgb(0x17, 0x18, 0x1a);
const CColor panel = rgb(0x1f, 0x21, 0x24);
const CColor slot = rgb(0x26, 0x28, 0x2c);           // empty channel slot
const CColor line = rgb(0xc8, 0xcb, 0xcf);           // panel print (white lines)
const CColor lineDim = rgb(0x5a, 0x5e, 0x64);
const CColor text = rgb(0xe6, 0xe8, 0xea);
const CColor dim = rgb(0x8c, 0x91, 0x98);
const CColor accent = rgb(0xf0, 0xa8, 0x3c);          // selected channel, EQ curve
const CColor chainCurve = rgb(0x9a, 0xc4, 0xe8);
const CColor ledGreenOn = rgb(0x5c, 0xe0, 0x5a), ledGreenOff = rgb(0x1c, 0x2a, 0x1c);
const CColor ledYellowOn = rgb(0xf2, 0xd0, 0x3a), ledYellowOff = rgb(0x2e, 0x2a, 0x16);
const CColor ledRedOn = rgb(0xff, 0x3a, 0x2e), ledRedOff = rgb(0x32, 0x16, 0x14);
const CColor buttonOff = rgb(0x34, 0x37, 0x3c), buttonOn = rgb(0xd8, 0xdb, 0xde);

// knob colours (one grey design for all knobs; the role is kept for the layout)
enum class KnobRole { Level, Freq, Trim, Global };
struct KnobColors { CColor body, edge, cap, pointer; };
inline KnobColors knobColors(KnobRole) {
  return { rgb(0x46, 0x49, 0x4f), rgb(0x30, 0x32, 0x36), rgb(0x52, 0x56, 0x5c), rgb(0xf0, 0xf0, 0xf0) };
}

inline VSTGUI::SharedPointer<VSTGUI::CFontDesc> font(double size, bool bold = false) {
#ifdef _WIN32
  const char* family = "Arial";
#else
  const char* family = "Liberation Sans";
#endif
  return VSTGUI::makeOwned<VSTGUI::CFontDesc>(family, size, bold ? VSTGUI::kBoldFace : VSTGUI::kNormalFace);
}

inline void label(VSTGUI::CDrawContext* c, const std::string& s, const VSTGUI::CRect& r, const CColor& col, double size,
                  bool bold = false, VSTGUI::CHoriTxtAlign align = VSTGUI::kCenterText) {
  c->setFont(font(size, bold));
  c->setFontColor(col);
  c->drawString(s.c_str(), r, align, true);
}

} // namespace theme
} // namespace eightbus
