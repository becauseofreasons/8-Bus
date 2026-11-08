#include "analyzer.h"
#include <algorithm>
#include <cmath>

namespace eightbus {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kFloor = -150;

// twiddle table for the largest size, smaller sizes use every (NLong / n)-th entry
const std::vector<std::complex<double>>& twiddles() {
  static const std::vector<std::complex<double>> t = [] {
    std::vector<std::complex<double>> v(Analyzer::NLong / 2);
    for (int k = 0; k < Analyzer::NLong / 2; k++) v[size_t(k)] = std::polar(1.0, -2 * kPi * k / Analyzer::NLong);
    return v;
  }();
  return t;
}

void fft(std::vector<std::complex<double>>& a, int n) {
  const std::vector<std::complex<double>>& tw = twiddles();
  for (int i = 1, j = 0; i < n; i++) {
    int bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[size_t(i)], a[size_t(j)]);
  }
  for (int len = 2; len <= n; len <<= 1) {
    const int stride = Analyzer::NLong / len;
    for (int k = 0; k < len / 2; k++) {
      const std::complex<double> w = tw[size_t(k * stride)];
      for (int i = 0; i < n; i += len) {
        const std::complex<double> u = a[size_t(i + k)], v = a[size_t(i + k + len / 2)] * w;
        a[size_t(i + k)] = u + v;
        a[size_t(i + k + len / 2)] = u - v;
      }
    }
  }
}

std::vector<double> hann(int n) {
  std::vector<double> w(static_cast<size_t>(n));
  for (int i = 0; i < n; i++) w[size_t(i)] = 0.5 - 0.5 * std::cos(2 * kPi * i / n);
  return w;
}
}

Analyzer::Analyzer(int p)
  : points(p), histIn(NLong, 0.0f), histOut(NLong, 0.0f), winLong(hann(NLong)), winShort(hann(NShort)), buf(NLong),
    powLong(NLong / 2), powShort(NShort / 2), target(size_t(p)), dbIn(size_t(p), kFloor), dbOut(size_t(p), kFloor),
    dbPeak(size_t(p), kFloor) {}

void Analyzer::clear() {
  std::fill(dbIn.begin(), dbIn.end(), kFloor);
  std::fill(dbOut.begin(), dbOut.end(), kFloor);
  std::fill(dbPeak.begin(), dbPeak.end(), kFloor);
  live = false;
}

void Analyzer::push(const float* in, const float* out, int n) {
  for (int i = 0; i < n; i++) {
    histIn[size_t(pos)] = in[i];
    histOut[size_t(pos)] = out[i];
    pos = (pos + 1) % NLong;
  }
  fresh += n;
}

// power per bin, scaled so that a sine of amplitude A summed over its bins gives A^2
void Analyzer::powerSpectrum(const std::vector<float>& hist, int n, const std::vector<double>& win, std::vector<double>& pw) {
  double w2 = 0;
  for (int i = 0; i < n; i++) {
    const double w = win[size_t(i)];
    buf[size_t(i)] = hist[size_t((pos - n + i + NLong) % NLong)] * w;      // the newest n samples
    w2 += w * w;
  }
  fft(buf, n);
  const double scale = 4.0 / (double(n) * w2);
  for (int k = 0; k < n / 2; k++) pw[size_t(k)] = std::norm(buf[size_t(k)]) * scale;
}

void Analyzer::bands(double fs, double (*freqAt)(int), bool tilt, std::vector<double>& out) {
  auto level = [&](const std::vector<double>& pw, int n, double f0, double f1, double f) {
    const double binHz = fs / n;
    const double k0 = f0 / binHz, k1 = f1 / binHz;
    double s = 0;
    if (k1 - k0 < 2.0) {
      // narrower than the resolution: interpolate (one bin of a Hann window holds 2/3 of a sine)
      const double x = f / binHz;
      const int k = std::max(1, std::min(n / 2 - 2, int(x)));
      const double t = x - k;
      s = 1.5 * ((1 - t) * pw[size_t(k)] + t * pw[size_t(k + 1)]);
    } else {
      const int a = std::max(1, int(std::floor(k0))), b = std::min(n / 2 - 1, int(std::ceil(k1)));
      for (int k = a; k <= b; k++) s += pw[size_t(k)];
    }
    return 10 * std::log10(s + 1e-30);
  };
  const double sixth = std::pow(2.0, 1.0 / 12);          // +-1/12 octave = 1/6 octave band
  for (int p = 0; p < points; p++) {
    const double f = freqAt(p);
    double f0 = p > 0 ? std::sqrt(f * freqAt(p - 1)) : f, f1 = p < points - 1 ? std::sqrt(f * freqAt(p + 1)) : f;
    f0 = std::min(f0, f / sixth);
    f1 = std::max(f1, f * sixth);
    double db;
    if (f <= 250) db = level(powLong, NLong, f0, f1, f);
    else if (f >= 500) db = level(powShort, NShort, f0, f1, f);
    else {
      const double t = std::log2(f / 250.0);            // 0..1 over one octave
      db = (1 - t) * level(powLong, NLong, f0, f1, f) + t * level(powShort, NShort, f0, f1, f);
    }
    if (tilt) db += 4.5 * std::log2(f / 1000.0);
    out[size_t(p)] = db;
  }
}

bool Analyzer::update(double fs, double ms, double (*freqAt)(int), const AnalyzerSettings& s) {
  if (s.mode == 0) {
    if (live) { clear(); return true; }
    return false;
  }
  const double release = s.speed == 0 ? 0.70 : s.speed == 1 ? 0.30 : 0.10;    // seconds
  const double aRise = 1 - std::exp(-ms / 12.0), aFall = 1 - std::exp(-ms / (release * 1000));
  const double peakFall = 6.0 * ms / 1000.0;            // dB per frame
  const bool haveAudio = fresh > 0;
  fresh = 0;
  bool any = false;
  for (int sig = 0; sig < 2; sig++) {
    std::vector<double>& db = sig == 0 ? dbOut : dbIn;
    if (sig == 1 && s.mode == 2) { std::fill(dbIn.begin(), dbIn.end(), kFloor); continue; }
    if (haveAudio) {
      const std::vector<float>& h = sig == 0 ? histOut : histIn;
      // the long (bass) window is 170 ms: renewing it every second frame is plenty
      if ((frame & 1) == 0 || !longValid[sig]) { powerSpectrum(h, NLong, winLong, powLongSig[sig]); longValid[sig] = true; }
      powLong = powLongSig[sig];
      powerSpectrum(h, NShort, winShort, powShort);
      bands(fs, freqAt, s.tilt, target);
    } else {
      std::fill(target.begin(), target.end(), kFloor);     // no audio (transport stopped): fall away
    }
    for (int p = 0; p < points; p++) {
      double& d = db[size_t(p)];
      const double t = target[size_t(p)];
      d += (t > d ? aRise : aFall) * (t - d);
      if (d > -140) any = true;
      if (sig == 0) {
        double& pk = dbPeak[size_t(p)];
        pk = std::max(d, pk - peakFall);
      }
    }
  }
  frame++;
  live = any;
  return any || haveAudio;
}

} // namespace eightbus
