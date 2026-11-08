#include "oversampler.h"
#include <cmath>
#include <cstring>

namespace r8 {

namespace {
double besselI0(double x) {
  double sum = 1, term = 1;
  for (int k = 1; k < 60; k++) {
    term *= (x / (2 * k)) * (x / (2 * k));
    sum += term;
    if (term < 1e-17 * sum) break;
  }
  return sum;
}
}

void Oversampler::prepare() {
  const double pi = 3.14159265358979323846, beta = 9.0;
  const int Ls[3] = { 2, 4, 8 };
  for (int i = 0; i < 3; i++) {
    Set& s = sets[i];
    s.L = Ls[i];
    s.taps = 36 * s.L + 1;
    s.h.assign(size_t(s.taps), 0.0);
    const int M = s.taps - 1;
    const double fc = 0.5 / s.L;                           // host Nyquist, relative to the high rate
    double sum = 0;
    for (int k = 0; k < s.taps; k++) {
      const double t = k - M / 2.0;
      const double sinc = t == 0 ? 2 * fc : std::sin(2 * pi * fc * t) / (pi * t);
      const double r = 2.0 * k / M - 1.0;
      const double w = besselI0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / besselI0(beta);
      s.h[size_t(k)] = sinc * w;
      sum += s.h[size_t(k)];
    }
    for (double& v : s.h) v /= sum;
    s.phase.assign(size_t(s.L), {});
    for (int p = 0; p < s.L; p++)
      for (int k = p; k < s.taps; k += s.L) s.phase[size_t(p)].push_back(s.h[size_t(k)] * s.L);
  }
  setFactor(L);
}

void Oversampler::setFactor(int f) {
  L = f <= 2 ? 2 : f <= 4 ? 4 : 8;
  cur = &sets[L == 2 ? 0 : L == 4 ? 1 : 2];
  reset();
}

void Oversampler::reset() {
  for (Ch& c : ch) c = Ch();
}

void Oversampler::up(int c, const double* in, int n, double* out) {
  Ch& h = ch[c];
  const int P = cur->L;
  for (int i = 0; i < n; i++) {
    h.upPos = (h.upPos + UpHist - 1) % UpHist;            // newest sample at the lowest index
    h.up[h.upPos] = h.up[h.upPos + UpHist] = in[i];
    const double* x = h.up + h.upPos;
    for (int p = 0; p < P; p++) {
      const std::vector<double>& ph = cur->phase[size_t(p)];
      double acc = 0;
      const int m = int(ph.size());
      for (int j = 0; j < m; j++) acc += ph[size_t(j)] * x[j];
      *out++ = acc;
    }
  }
}

void Oversampler::down(int c, const double* in, int n, double* out) {
  Ch& h = ch[c];
  const int P = cur->L, N = cur->taps;
  const double* hh = cur->h.data();
  for (int i = 0; i < n; i++) {
    for (int p = 0; p < P; p++) {
      h.downPos = (h.downPos + DownHist - 1) % DownHist;
      h.down[h.downPos] = h.down[h.downPos + DownHist] = *in++;
      if (p == 0) {                       // output on the host grid: aligned to the first of the L samples
        const double* x = h.down + h.downPos;
        double acc = 0;
        for (int k = 0; k < N; k++) acc += hh[k] * x[k];
        out[i] = acc;
      }
    }
  }
}

} // namespace r8
