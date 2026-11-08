// Editor options, saved with the project (controller state)
#pragma once

namespace eightbus {

struct UiOptions {
  double zoom = 1.0;
  int selected = 0;                // channel shown in the EQ curve
  // spectrum analyzer
  int anMode = 1;                  // 0 off, 1 input + output, 2 output only
  int anSpeed = 1;                 // 0 slow, 1 medium, 2 fast
  int anRange = 1;                 // 60 / 90 / 120 dB
  bool anTilt = true;              // 4.5 dB/octave
  bool anPeak = true;              // peak line
};

} // namespace eightbus
