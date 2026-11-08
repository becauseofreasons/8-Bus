// Linear circuits (resistors, capacitors, ideal op-amps, one voltage input) and their digital form.
//
// A section of the channel strip is written down as a netlist, exactly like the schematic. From it:
//  - response(): the analog frequency response (for the EQ curve, the knob read-outs, the tests)
//  - discretize(): a state-space filter (trapezoidal rule, "DK method"): one state per capacitor.
//    Changing a resistor keeps the states physically meaningful, so pots can move while audio runs.
// Everything has fixed capacity and does not allocate: it is rebuilt on the audio thread when a pot moves.
#pragma once
#include <complex>

namespace r8 {

constexpr int GND = -1;

struct Netlist {
  static constexpr int MaxNodes = 24, MaxR = 40, MaxC = 6, MaxOA = 6;
  struct Res { int a, b; double r; };
  struct Cap { int a, b; double c; };
  struct OpAmp { int p, m, o; };      // non-inverting input, inverting input, output (ideal: v+ = v-)

  int nodes = 0;
  Res R[MaxR]; int nr = 0;
  Cap C[MaxC]; int nc = 0;
  OpAmp OA[MaxOA]; int noa = 0;
  int in = -1, out = -1;

  int node() { return nodes++; }
  void r(int a, int b, double v) { R[nr++] = { a, b, v < 1.0 ? 1.0 : v }; }   // a pot half never gets 0 ohm
  void c(int a, int b, double v) { C[nc++] = { a, b, v }; }
  void opamp(int p, int m, int o) { OA[noa++] = { p, m, o }; }
};

// analog transfer function out/in at frequency f (Hz)
std::complex<double> response(const Netlist& n, double f);

// state-space filter with K = number of capacitors (fixed per section)
template <int K> struct DkFilter {
  double A[K][K] = {}, B[K] = {}, C[K] = {}, D = 0;
  // per channel state
  struct State { double s[K] = {}; };
  inline double tick(State& st, double u) const {
    double y = D * u;
    for (int j = 0; j < K; j++) y += C[j] * st.s[j];
    double ns[K];
    for (int i = 0; i < K; i++) {
      double v = B[i] * u;
      for (int j = 0; j < K; j++) v += A[i][j] * st.s[j];
      ns[i] = v;
    }
    for (int i = 0; i < K; i++) st.s[i] = ns[i];
    return y;
  }
};

// fills A (K*K row major), B, C, D; returns false if the circuit is singular or K does not match
bool discretize(const Netlist& n, double fs, int K, double* A, double* B, double* C, double& D);

template <int K> bool discretize(const Netlist& n, double fs, DkFilter<K>& f) {
  return discretize(n, fs, K, &f.A[0][0], f.B, f.C, f.D);
}

} // namespace r8
