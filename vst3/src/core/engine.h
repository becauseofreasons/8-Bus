// The whole plugin DSP without the VST3 SDK: input gain -> oversampling -> 1..8 channel strips in
// series (direct out of one into the line in of the next) -> down sampling -> output gain.
// Parameters arrive as normalized values (ids from params.h) and are smoothed here.
#pragma once
#include "oversampler.h"
#include "params.h"
#include "strip.h"
#include "supply.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace r8 {

struct Meters {
  float in[2] = {}, out[2] = {};      // peak since the last read, linear (1.0 = 0 dBFS)
  float ol[kMaxStrips] = {};          // OL detector voltage (lit above Strip::OlThreshold)
  int strips = 1;
};

class Engine {
public:
  static constexpr int Block = 32;     // control rate (host samples)

  Engine();
  void prepare(double sampleRate, int maxBlock);   // allocates
  void reset();
  void setParam(uint32_t id, double norm);         // audio thread (or before processing)
  double param(uint32_t id) const;
  void setSeed(uint32_t s);                        // tolerance set of this instance
  uint32_t seed() const { return toleranceSeed; }

  // in and out may be the same buffers; nch 1 or 2
  void process(const float* const* in, float* const* out, int nch, int n);

  int latency() const { return Oversampler::Latency; }
  int strips() const { return activeStrips; }
  void takeMeters(Meters& m);                      // audio thread: peaks since the last call

  // for tests: one strip and its controls
  Strip& strip(int i) { return chain[i]; }

private:
  double fs = 48000;
  int maxBlock = 0;
  std::vector<double> norm;               // indexed by paramIndex()
  std::vector<double> smooth;             // smoothed strip pots (same index)
  double inGain = 1, outGain = 1;         // linear, smoothed per block
  double smoothCoef = 0.1;
  int activeStrips = 1, osFactor = 4;
  bool tolerances = false, bypass = false;
  double bypassMix = 0;
  uint32_t toleranceSeed = 1;
  bool needTolerance = true;

  Oversampler os;
  Strip chain[kMaxStrips];
  Supply supply[2];                        // one console per audio channel
  std::vector<double> base, high;          // block buffers (host rate, oversampled)
  std::vector<float> dry[2];               // bypass: input delayed by the latency
  int dryPos = 0;

  // output coupling: the console's main output / the recorder's input block sub-sonic swings
  // (2nd order high pass, 5 Hz). Without it the DAW gets the "charge" of the coupling capacitors.
  struct OutHp {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {};
    void set(double fc, double fs);
    void reset();
    inline double tick(int c, double x) {
      const double y = b0 * x + b1 * x1[c] + b2 * x2[c] - a1 * y1[c] - a2 * y2[c];
      x2[c] = x1[c]; x1[c] = x; y2[c] = y1[c]; y1[c] = y;
      return y;
    }
  } outHp;
  static constexpr double OutHpHz = 5.0;

  Meters meters;
  void applyTolerance();
  void controlStep(int n);
  StripControls controlsOf(int s) const;
};

} // namespace r8
