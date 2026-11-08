// Live spectrum of input and output for the EQ curve view (editor thread only).
// - two resolutions: a long FFT (8192) for the bass (exact pitch of a kick), a short one (2048) for fast
//   highs; cross faded between 250 and 500 Hz. Recomputed every frame (overlapping windows).
// - power averaged per display band (at least 1/6 octave), Hann window; a full scale sine reads 0 dBFS
// - optional tilt 4.5 dB/octave around 1 kHz (pink noise lies flat)
// - smoothing in time: fast rise, release by "speed"; peak line falling slowly
#pragma once
#include <complex>
#include <vector>

namespace eightbus {

struct AnalyzerSettings {
  int mode = 1;          // 0 off, 1 input + output, 2 output only
  int speed = 1;         // 0 slow, 1 medium, 2 fast
  int range = 1;         // 0: 60 dB, 1: 90 dB, 2: 120 dB
  bool tilt = true;      // 4.5 dB/octave
  bool peak = true;      // peak line
  double rangeDb() const { return range == 0 ? 60 : range == 1 ? 90 : 120; }
};

class Analyzer {
public:
  static constexpr int NLong = 8192, NShort = 2048;
  explicit Analyzer(int points);
  void push(const float* in, const float* out, int n);      // newest samples
  // recompute the display bands; returns true when the display changed
  bool update(double sampleRate, double ms, double (*freqAt)(int), const AnalyzerSettings& s);
  const std::vector<double>& input() const { return dbIn; }
  const std::vector<double>& output() const { return dbOut; }
  const std::vector<double>& peak() const { return dbPeak; }
  bool active() const { return live; }
  void clear();

private:
  int points;
  std::vector<float> histIn, histOut;      // ring of the last NLong samples
  int pos = 0, fresh = 0;
  std::vector<double> winLong, winShort;
  std::vector<std::complex<double>> buf;
  std::vector<double> powLong, powShort;   // power spectra of the current frame
  std::vector<double> target, dbIn, dbOut, dbPeak;
  std::vector<double> powLongSig[2] = { std::vector<double>(NLong / 2), std::vector<double>(NLong / 2) };
  bool longValid[2] = { false, false };
  unsigned frame = 0;
  bool live = false;
  void powerSpectrum(const std::vector<float>& hist, int n, const std::vector<double>& win, std::vector<double>& pw);
  void bands(double fs, double (*freqAt)(int), bool tilt, std::vector<double>& out);
};

} // namespace eightbus
