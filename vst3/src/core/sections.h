// The channel strip of the 8-bus console as netlists (from the schematic of the channel board).
// Keep in step with tools/circuit_ref.py (the reference for the curves, checked by test/curves.cpp).
//
// Pots are given as rotation 0..1 (0 = fully counter clockwise):
//   gain pots (lin. B taper): 0.5 = flat, 1 = +15 dB
//   frequency pots (dual rheostats, log taper): 0 = lowest frequency
//   bandwidth pot: 0 = 3 octaves, 0.5 = 2 octaves, 1 = 1/12 octave
#pragma once
#include "circuit.h"
#include <cstdint>

namespace r8 {

constexpr double kPi = 3.14159265358979323846;

// component tolerances of one strip: multipliers for the fixed resistors and capacitors
struct Tolerance {
  double r[32];
  double c[12];
  double gang[4];      // tracking error of the dual frequency pots (second gang)
  Tolerance() { reset(); }
  void reset() {
    for (double& v : r) v = 1;
    for (double& v : c) v = 1;
    for (double& v : gang) v = 1;
  }
  void randomize(uint32_t seed);   // resistors 1 %, capacitors 5 %, pot gangs 3 %
};

// taper of a log ("audio") pot: resistance share at travel x (0..1), 15 % at mid travel
double taperLog(double x, double mid = 0.15);

// --- section netlists (input node = in, output node = out) --------------------------------
void buildLoCut(Netlist& n, const Tolerance& t);                                     // 3 caps
void buildLoMid(Netlist& n, double gain, double freq, const Tolerance& t);           // 2 caps, U2B
void buildHiMid(Netlist& n, double gain, double freq, double bw, const Tolerance& t); // 3 caps, U3A + SVF
void buildHiLo(Netlist& n, double hi, double lo, const Tolerance& t);                // 4 caps, U1B (inverting)

constexpr int kLoCutCaps = 3, kLoMidCaps = 2, kHiMidCaps = 3, kHiLoCaps = 4;

// --- pots ----------------------------------------------------------------------------------
double loMidFreqHz(double rot);          // Wien bridge centre
double hiMidFreqHz(double rot);          // tuning of the state variable filter
double hiMidLowerOhm(double bw);         // R115: resistance between P7 and the wiper
double hiMidBwOct(double bw);            // nominal bandwidth in octaves
double hiMidQ(double bw);

// pre-amp (board 08): line input pad + current feedback gain stage, gain set by R166 TRIM (5K).
// The input pair puts the voltage at the bases across the gain resistor Rg (R113 22R + TRIM) in series
// with C107 470 uF; the current through it is limited by the bias of each half (2K49 from +16 V).
// U1A turns the current into the output voltage (2 x 3K92).
constexpr double kPreampPad = 3920.0 / (3920.0 + 7320.0);       // line pad 7K32 / 3K92 per leg
constexpr double kPreampTransimpedance = 2 * 3920.0;            // volts per ampere
constexpr double kPreampBias = (16.0 - 0.65) / 2490.0;          // max. current through Rg (6.2 mA)
constexpr double kPreampC107 = 470e-6;
double preampRg(double trimRot);        // gain resistor in ohm
struct PreampGain {
  double gain;      // line in -> U1A out, high frequencies
  double hpHz;      // corner of the trim network (C107 in series with the gain resistor)
};
PreampGain preampGain(double trimRot);
double trimDb(double trimRot);          // line gain in dB
double trimUnityRot();                  // trim rotation with 0 dB line gain

// channel fader (10K, +10 dB make-up stage U102B): total gain in dB, -inf at 0
double faderDb(double pos);
double faderPosOfDb(double db);
constexpr double kFaderUnity = 0.75;

// --- analog read-outs for knobs and the EQ curve ---------------------------------------------
double loMidGainDb(double gain, double freq);      // at the centre frequency
double hiMidGainDb(double gain, double freq, double bw);
double hiGainDb(double hi);                         // at 12 kHz
double loGainDb(double lo);                         // at 80 Hz

// levels: digital 1.0 = 8.69 V peak (0 dBu = -18 dBFS)
constexpr double kVoltsPerUnit = 8.6856;            // 0.775 V * sqrt(2) * 10^(18/20)
constexpr double kVoltsDbu0Peak = 1.0960;           // peak of a 0 dBu sine

} // namespace r8
