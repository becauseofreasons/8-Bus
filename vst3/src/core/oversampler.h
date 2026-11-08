// 2x / 4x / 8x oversampling with linear phase FIR filters (Kaiser windowed sinc, about 90 dB).
// Filter length 36*L+1: up and down sampling together delay by exactly 36 samples at the host rate,
// for every factor - the latency the plugin reports never changes.
#pragma once
#include <vector>

namespace r8 {

class Oversampler {
public:
  static constexpr int MaxCh = 2;
  static constexpr int Latency = 36;          // host samples (up + down)

  void prepare();                             // allocates the filters for 2x, 4x and 8x
  void setFactor(int L);                      // 2, 4 or 8; clears the history (no allocation)
  int factor() const { return L; }
  void reset();

  // in: n host samples -> out: n*L samples
  void up(int ch, const double* in, int n, double* out);
  // in: n*L samples -> out: n host samples
  void down(int ch, const double* in, int n, double* out);

private:
  struct Set {
    int L = 2, taps = 0;
    std::vector<double> h;                    // prototype filter (gain 1)
    std::vector<std::vector<double>> phase;   // polyphase parts for the up sampler (gain L)
  };
  Set sets[3];
  Set* cur = nullptr;
  int L = 4;
  // histories (double length, so a window is always contiguous)
  static constexpr int UpHist = 40, DownHist = 8 * 36 + 1;
  struct Ch {
    double up[2 * UpHist] = {};
    int upPos = 0;
    double down[2 * DownHist] = {};
    int downPos = 0;
  } ch[MaxCh];
};

} // namespace r8
