// Checks the C++ circuits against the Python reference (tools/circuit_ref.py --table test/ref_curves.txt)
// and the digital filters (DK method) against the analog response.
//   r8_curves <ref_curves.txt>
#include "circuit.h"
#include "sections.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace r8;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) failures++;
}

static void build(const std::string& sec, double a, double b, double c, Netlist& n) {
  Tolerance t;
  if (sec == "locut") buildLoCut(n, t);
  else if (sec == "mid1") buildLoMid(n, a, b, t);
  else if (sec == "mid2") buildHiMid(n, a, b, c, t);
  else buildHiLo(n, a, b, t);
}

// amplitude and phase of a digital filter at f: run a sine into it, correlate after settling
template <int K> static std::complex<double> measure(const DkFilter<K>& f, double fs, double hz) {
  typename DkFilter<K>::State st;
  const double w = 2 * kPi * hz / fs;
  const int settle = int(fs * 0.5), len = int(fs * 0.2);
  for (int i = 0; i < settle; i++) f.tick(st, std::sin(w * i));
  // least squares fit y = a sin + b cos (no leakage from a window with fractional periods)
  double ss = 0, cc = 0, sc = 0, ys = 0, yc = 0;
  for (int i = settle; i < settle + len; i++) {
    const double s = std::sin(w * i), c = std::cos(w * i), y = f.tick(st, s);
    ss += s * s; cc += c * c; sc += s * c; ys += y * s; yc += y * c;
  }
  const double det = ss * cc - sc * sc;
  const double a = (ys * cc - yc * sc) / det, b = (yc * ss - ys * sc) / det;
  return std::complex<double>(a, b);    // y = Re{(a + jb) e^{jwt}} relative to sin: a sin + b cos
}

template <int K> static double dkError(const Netlist& n, double fs) {
  DkFilter<K> f;
  if (!discretize(n, fs, f)) return 1e9;
  double worst = 0;
  for (double hz : { 31.0, 80.0, 250.0, 1000.0, 3000.0, 9000.0 }) {
    const double fa = fs / kPi * std::tan(kPi * hz / fs);        // trapezoidal rule: analog frequency
    const std::complex<double> d = measure(f, fs, hz), a = response(n, fa);
    const double e = std::abs(20 * std::log10(std::abs(d) / std::abs(a)));
    const double ph = std::abs(std::arg(d / a)) * 180 / kPi;
    worst = std::max(worst, std::max(e, ph / 10));                 // 0.1 dB ~ 1 degree
  }
  return worst;
}

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "test/ref_curves.txt";
  std::ifstream in(path);
  if (!in) { std::printf("Referenz nicht gefunden: %s\n", path); return 2; }
  std::printf("Analoge Kurven gegen Python-Referenz:\n");
  std::string line;
  int n = 0;
  double worstDb = 0, worstDeg = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream s(line);
    std::string sec;
    double a, b, c, f, re, im;
    s >> sec >> a >> b >> c >> f >> re >> im;
    Netlist net;
    build(sec, a, b, c, net);
    const std::complex<double> h = response(net, f), ref(re, im);
    const double edb = std::abs(20 * std::log10(std::abs(h) / std::abs(ref)));
    const double edeg = std::abs(std::arg(h / ref)) * 180 / kPi;
    if (edb > worstDb || edeg > worstDeg) {
      if (edb > 0.01 || edeg > 0.1) std::printf("    %s %.2f %.2f %.2f %8.1f Hz: %.4f dB %.3f Grad\n", sec.c_str(), a, b, c, f, edb, edeg);
    }
    worstDb = std::max(worstDb, edb);
    worstDeg = std::max(worstDeg, edeg);
    n++;
  }
  std::printf("  %d Punkte, größte Abweichung %.5f dB / %.4f Grad\n", n, worstDb, worstDeg);
  check(n > 400, "Referenztabelle gelesen");
  check(worstDb < 0.01 && worstDeg < 0.1, "C++-Schaltungen = Python-Referenz");

  std::printf("Digitale Filter (192 kHz) gegen analoge Kurve:\n");
  const double fs = 192000;
  Tolerance t;
  {
    Netlist net; buildLoCut(net, t);
    const double e = dkError<kLoCutCaps>(net, fs);
    std::printf("  Lo Cut: %.4f\n", e); check(e < 0.05, "Lo Cut digital = analog");
  }
  for (double g : { 0.0, 1.0 }) for (double fr : { 0.0, 0.5, 1.0 }) {
    Netlist net; buildLoMid(net, g, fr, t);
    const double e = dkError<kLoMidCaps>(net, fs);
    char w[80]; std::snprintf(w, sizeof w, "Lo Mid g=%.0f f=%.1f digital = analog (%.4f)", g, fr, e);
    check(e < 0.05, w);
  }
  for (double g : { 0.0, 1.0 }) for (double bw : { 0.0, 0.5, 0.9 }) {
    Netlist net; buildHiMid(net, g, 0.4, bw, t);
    const double e = dkError<kHiMidCaps>(net, fs);
    char w[80]; std::snprintf(w, sizeof w, "Hi Mid g=%.0f bw=%.1f digital = analog (%.4f)", g, bw, e);
    check(e < 0.05, w);
  }
  for (double hi : { 0.0, 1.0 }) for (double lo : { 0.0, 1.0 }) {
    Netlist net; buildHiLo(net, hi, lo, t);
    const double e = dkError<kHiLoCaps>(net, fs);
    char w[80]; std::snprintf(w, sizeof w, "Hi/Lo hi=%.0f lo=%.0f digital = analog (%.4f)", hi, lo, e);
    check(e < 0.05, w);
  }

  std::printf("Regler und Daten:\n");
  std::printf("  Lo Mid %.0f .. %.0f Hz (Mitte %.0f), Hi Mid %.0f .. %.0f Hz (Mitte %.0f)\n", loMidFreqHz(0), loMidFreqHz(1),
              loMidFreqHz(0.5), hiMidFreqHz(0), hiMidFreqHz(1), hiMidFreqHz(0.5));
  std::printf("  Bandbreite %.2f / %.2f / %.3f Oktaven, Q %.2f .. %.1f\n", hiMidBwOct(0), hiMidBwOct(0.5), hiMidBwOct(1), hiMidQ(0), hiMidQ(1));
  std::printf("  Lo Mid %+.1f / %+.1f dB, Hi Mid %+.1f / %+.1f dB, Hi %+.1f / %+.1f dB, Lo %+.1f / %+.1f dB\n",
              loMidGainDb(1, 0.5), loMidGainDb(0, 0.5), hiMidGainDb(1, 0.5, 0.5), hiMidGainDb(0, 0.5, 0.5),
              hiGainDb(1), hiGainDb(0), loGainDb(1), loGainDb(0));
  std::printf("  Trim (Line) %+.1f .. %+.1f dB, 0 dB bei %.3f\n", trimDb(0), trimDb(1), trimUnityRot());
  check(std::abs(loMidFreqHz(0) - 50) < 5 && std::abs(loMidFreqHz(1) - 3100) < 100, "Lo Mid 45 Hz .. 3 kHz (Handbuch)");
  check(std::abs(hiMidFreqHz(1) - 18000) < 500 && std::abs(hiMidFreqHz(0) - 470) < 50, "Hi Mid 500 Hz .. 18 kHz (Handbuch)");
  check(std::abs(hiMidBwOct(0) - 3) < 0.01 && std::abs(hiMidBwOct(0.5) - 2) < 0.01 && std::abs(hiMidBwOct(1) - 1.0 / 12) < 0.002,
        "Bandbreite 3 / 2 / 1/12 Oktave (Handbuch)");
  check(std::abs(loMidGainDb(1, 0.5) - 15) < 1.5 && std::abs(hiMidGainDb(1, 0.5, 0.5) - 15) < 1.5, "Mitten +-15 dB");
  check(std::abs(hiGainDb(1) - 15) < 1.5 && std::abs(loGainDb(1) - 15) < 1.5, "Shelves +-15 dB");

  std::printf(failures ? "FEHLER: %d Prüfungen\n" : "alle Prüfungen ok\n", failures);
  return failures ? 1 : 0;
}
