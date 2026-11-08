// Engine test without a DAW: level, latency, clipping, chains, automation, CPU load.
// Writes WAV files of a synthetic 909-style kick through the console when an output folder is given.
//   r8_enginetest [out-folder]
#include "engine.h"
#include "sections.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace r8;

static const double FS = 48000;
static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) failures++;
}

static void writeWav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
  std::ofstream f(path, std::ios::binary);
  auto u32 = [&](uint32_t v) { f.write((const char*)&v, 4); };
  auto u16 = [&](uint16_t v) { f.write((const char*)&v, 2); };
  const uint32_t n = uint32_t(l.size());
  f.write("RIFF", 4); u32(36 + n * 8); f.write("WAVEfmt ", 8); u32(16); u16(3); u16(2);
  u32(uint32_t(FS)); u32(uint32_t(FS * 8)); u16(8); u16(32); f.write("data", 4); u32(n * 8);
  for (uint32_t i = 0; i < n; i++) { f.write((const char*)&l[i], 4); f.write((const char*)&r[i], 4); }
  std::printf("  geschrieben: %s\n", path.c_str());
}

// 909-like kick: pitch 230 -> 50 Hz, body decay ~0.5 s, short click; repeats every 0.5 s
static std::vector<float> kick(double seconds, double peak) {
  std::vector<float> x(size_t(seconds * FS), 0.0f);
  const double period = 0.5;
  for (size_t i = 0; i < x.size(); i++) {
    const double t = std::fmod(i / FS, period);
    // phase = integral of f(t) = 50 + 180 exp(-t/0.025)
    const double ph = 2 * kPi * (50 * t + 180 * 0.025 * (1 - std::exp(-t / 0.025)));
    const double body = std::sin(ph) * std::exp(-t / 0.18);
    const double click = std::exp(-t / 0.0015) * 0.3;
    x[i] = float(peak * (body + click) / 1.05);
  }
  return x;
}

// one 909-like hit per second with a natural decay; cut > 0: the sample ends there (digital silence)
static std::vector<float> kickHit(double seconds, double decay, double cut = 0) {
  std::vector<float> x(size_t(seconds * FS), 0.0f);
  for (size_t i = 0; i < x.size(); i++) {
    const double t = std::fmod(i / FS, 1.0);
    const double ph = 2 * kPi * (50 * t + 160 * 0.02 * (1 - std::exp(-t / 0.02)));
    const double env = (cut > 0 && t > cut) ? 0.0 : std::exp(-t / decay);
    x[i] = float((std::sin(ph) * env + 0.25 * std::exp(-t / 0.001)) / 1.1);
  }
  return x;
}

struct Run {
  Engine e;
  explicit Run(int os = 4) {
    e.setParam(kParamOversampling, os == 2 ? 0.0 : os == 4 ? 0.5 : 1.0);
    e.prepare(FS, 512);
  }
  void process(const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& outL, std::vector<float>& outR, int block = 512) {
    outL.assign(inL.size(), 0); outR.assign(inR.size(), 0);
    for (size_t p = 0; p < inL.size(); p += size_t(block)) {
      const int n = int(std::min<size_t>(size_t(block), inL.size() - p));
      const float* in[2] = { inL.data() + p, inR.data() + p };
      float* out[2] = { outL.data() + p, outR.data() + p };
      e.process(in, out, 2, n);
    }
  }
};

static double peakOf(const std::vector<float>& x, size_t from = 0) {
  double p = 0;
  for (size_t i = from; i < x.size(); i++) p = std::max(p, double(std::fabs(x[i])));
  return p;
}
static double rmsOf(const std::vector<float>& x, size_t from, size_t to) {
  double q = 0;
  for (size_t i = from; i < to; i++) q += double(x[i]) * x[i];
  return std::sqrt(q / double(to - from));
}

int main(int argc, char** argv) {
  const std::string outDir = argc > 1 ? argv[1] : "";

  std::printf("Pegel und Latenz (1 Kanal, alles neutral):\n");
  {
    Run r;
    std::vector<float> x(static_cast<size_t>(FS)), y, yr;
    for (size_t i = 0; i < x.size(); i++) x[i] = float(0.1 * std::sin(2 * kPi * 1000 * i / FS));
    r.process(x, x, y, yr);
    const double gain = 20 * std::log10(rmsOf(y, 24000, 48000) / rmsOf(x, 24000, 48000));
    double err = 0;
    for (size_t i = 24000; i < 48000; i++) err += std::pow(double(y[i]) - x[i - 36], 2);
    const double errDb = 10 * std::log10(err / 24000) - 20 * std::log10(rmsOf(x, 24000, 48000));
    std::printf("  1 kHz bei -20 dBFS: %+.2f dB, Abweichung zum um 36 Samples verzögerten Eingang %.1f dB\n", gain, errDb);
    check(std::fabs(gain) < 0.3, "neutraler Kanal: Pegel 0 dB (+-0.3)");
    check(errDb < -30, "Latenz genau 36 Samples (wie gemeldet)");
    check(r.e.latency() == 36, "gemeldete Latenz 36");
  }

  std::printf("Übersteuern (909-Kick, 0 dBFS):\n");
  const std::vector<float> k = kick(4.0, 1.0);
  std::vector<float> y1, y1r, y4, y4r;
  {
    Run r;
    r.e.setParam(stripParamId(0, kLo), 1.0);          // LO +15
    r.e.setParam(stripParamId(0, kFader), 1.0);       // fader +10
    r.process(k, k, y1, y1r);
    const double p = peakOf(y1, 4800);
    std::printf("  1 Kanal LO +15, Fader +10: Spitze %.3f (%.1f dBFS), Schiene = %.3f\n", p, 20 * std::log10(p), Strip::RailPos / kVoltsPerUnit);
    // the output coupling removes the DC of the asymmetric clipped kick: peaks may grow by that much
    check(p < Strip::RailPos / kVoltsPerUnit * 1.3 && p > 1.2, "Ausgang begrenzt (Op-Amp-Schiene + Verschiebung durch die Ausgangskopplung)");
    Meters m; r.e.takeMeters(m);
    std::printf("  OL-Detektor %.2f V (leuchtet ab %.2f V)\n", m.ol[0], Strip::OlThreshold);
    check(m.ol[0] > Strip::OlThreshold, "OL-LED leuchtet");
  }
  {
    Run r;
    r.e.setParam(kParamStrips, normOfStrips(4));
    for (int s = 0; s < 4; s++) {
      r.e.setParam(stripParamId(s, kLo), 0.85);
      r.e.setParam(stripParamId(s, kHmGain), 0.7);
      r.e.setParam(stripParamId(s, kHmFreq), 0.35);
      r.e.setParam(stripParamId(s, kFader), 0.85);
    }
    r.e.setParam(kParamOutGain, 0.5 - 6.0 / 48);
    r.process(k, k, y4, y4r);
    const double p = peakOf(y4, 4800);
    bool finite = true;
    for (float v : y4) if (!std::isfinite(v)) finite = false;
    std::printf("  4 Kanäle in Reihe: Spitze %.3f\n", p);
    check(finite && p < 2.0, "Kette bleibt endlich und begrenzt");
  }
  {
    Run quiet;
    std::vector<float> x(static_cast<size_t>(FS)), y, yr;
    for (size_t i = 0; i < x.size(); i++) x[i] = float(0.05 * std::sin(2 * kPi * 100 * i / FS));
    quiet.e.setParam(stripParamId(0, kLo), 1.0);
    quiet.process(x, x, y, yr);
    const double g = 20 * std::log10(rmsOf(y, 24000, 48000) / rmsOf(x, 24000, 48000));
    std::printf("  leise 100 Hz mit LO +15: %+.1f dB (Kurve analog: %+.1f dB)\n", g, loGainDb(1.0) - 1.6);
    check(g > 8 && g < 14, "LO-Anhebung bei 100 Hz wirkt");
  }

  std::printf("Tail und Ladung (eine Kick pro Sekunde, natürlicher Ausklang 0.12 s):\n");
  {
    // how long the third hit stays within 6 dB of its start (50 ms steps)
    auto hold = [](const std::vector<float>& y) {
      const size_t a = size_t(2 * FS) + 36;
      double first = 0;
      for (int w = 0; w < 20; w++) {
        double q = 0;
        for (size_t i = a + size_t(w) * 2400; i < a + size_t(w + 1) * 2400; i++) q += double(y[i]) * y[i];
        const double db = 10 * std::log10(q / 2400 + 1e-20);
        if (w == 0) first = db; else if (db < first - 6) return w * 50;
      }
      return 1000;
    };
    std::vector<float> x = kickHit(3.0, 0.12), yu, yr, ym, yh;
    Run unity; unity.process(x, x, yu, yr);
    Run maxTrim; maxTrim.e.setParam(stripParamId(0, kTrim), 1.0); maxTrim.process(x, x, ym, yr);
    Run hot; hot.e.setParam(stripParamId(0, kTrim), 1.0); hot.e.setParam(stripParamId(0, kLo), 1.0); hot.e.setParam(stripParamId(0, kFader), 1.0);
    hot.process(x, x, yh, yr);
    std::printf("  Kick hält (innerhalb 6 dB): Trim 0 dB %d ms, Trim voll %d ms, Trim voll + LO +15 + Fader +10 %d ms\n", hold(yu), hold(ym), hold(yh));
    check(hold(ym) >= 3 * hold(yu) && hold(ym) >= 400, "Trim voll verlängert die Kick (Tail)");
    double mean = 0;
    for (size_t i = size_t(FS); i < size_t(3 * FS); i++) mean += yh[i];
    mean /= 2 * FS;
    std::printf("  Gleichanteil am Ausgang (heiß): %+.4f\n", mean);
    check(std::fabs(mean) < 0.01, "keine Ladung (Gleichanteil) am Ausgang");
    // the sample ends at 0.35 s: what is left after it must die away, not hang at the rail
    std::vector<float> xc = kickHit(3.0, 0.12, 0.35), yc;
    // ideal supply: this checks the coupling capacitors only (with a typical supply the console flutters on)
    Run cut; cut.e.setParam(kParamSupply, 0.0); cut.e.setParam(stripParamId(0, kTrim), 1.0); cut.e.setParam(stripParamId(0, kLo), 1.0); cut.e.setParam(stripParamId(0, kFader), 1.0);
    cut.process(xc, xc, yc, yr);
    const size_t b = size_t((2 + 0.35) * FS) + 36;
    double late = 0, rail = 0;
    for (size_t i = b + size_t(0.15 * FS); i < b + size_t(0.6 * FS); i++) late += double(yc[i]) * yc[i];
    for (size_t i = b + size_t(0.05 * FS); i < b + size_t(0.6 * FS); i++) rail += std::fabs(yc[i]) > 1.0 ? 1 : 0;
    late = 10 * std::log10(late / (0.45 * FS) + 1e-20);
    std::printf("  Sample endet hart: danach %.0f ms über 0 dBFS, Rest 150..600 ms danach %.1f dB\n", rail / FS * 1000, late);
    check(rail == 0 && late < -30, "nach dem Sample-Ende klingt es aus (kein Hängen an der Schiene)");
    if (!outDir.empty()) {
      writeWav(outDir + "/kick_natur_dry.wav", x, x);
      writeWav(outDir + "/kick_natur_trim_0dB.wav", yu, yu);
      writeWav(outDir + "/kick_natur_trim_voll.wav", ym, ym);
      writeWav(outDir + "/kick_natur_trim_voll_lo15_fader10.wav", yh, yh);
      writeWav(outDir + "/kick_sample_endet_hart_heiss.wav", yc, yc);
    }
  }

  std::printf("Flattern über Netzteil/Masse (5 Kanäle, kurze Kick alle 400 ms):\n");
  {
    // short 909-like kick: over after 180 ms
    std::vector<float> x(size_t(FS * 5.2));
    for (size_t i = 0; i < x.size(); i++) {
      const double t = std::fmod(i / FS, 0.4);
      const double ph = 2 * kPi * (54.3 * t + 200 * 0.03 * (1 - std::exp(-t / 0.03)));
      double env = std::exp(-t / 0.05); if (t > 0.18 || i >= size_t(4.0 * FS)) env = 0;
      x[i] = float(0.25 * (std::sin(ph) * env + (i < size_t(4.0 * FS) ? 0.2 * std::exp(-t / 0.001) : 0.0)));
    }
    auto run = [&](double supply, double t1, double t2, std::vector<float>& y) {
      Run r;
      r.e.setParam(kParamSupply, supply);
      r.e.prepare(FS, 512);
      r.e.setParam(kParamStrips, normOfStrips(5));
      r.e.setParam(stripParamId(0, kTrim), t1);
      r.e.setParam(stripParamId(1, kTrim), t2);
      std::vector<float> yr;
      r.process(x, x, y, yr);
    };
    // level (dB) of the last kick at ms after its start, relative to its first 40 ms
    auto at = [](const std::vector<float>& y, double ms) {
      const size_t a = size_t(3.6 * FS) + 36;
      auto rms = [&](size_t b) { double q = 0; for (size_t i = b; i < b + 1920; i++) q += double(y[i]) * y[i]; return 10 * std::log10(q / 1920 + 1e-20); };
      return rms(a + size_t(ms / 1000 * FS)) - rms(a);
    };
    const double u = trimUnityRot();
    std::vector<float> idleI, idleT, oneI, oneT, twoI, twoT;
    run(0.0, u, u, idleI); run(0.5, u, u, idleT);
    run(0.0, 0.85, u, oneI); run(0.5, 0.85, u, oneT);
    run(0.0, 0.85, 0.6, twoI); run(0.5, 0.85, 0.6, twoT);
    std::printf("  Pegel 160 ms / 360 ms nach dem Anschlag (ideal -> typisch):\n");
    std::printf("    Trims entspannt:          %5.1f / %5.1f  ->  %5.1f / %5.1f dB\n", at(idleI, 160), at(idleI, 360), at(idleT, 160), at(idleT, 360));
    std::printf("    erster Trim heiß:         %5.1f / %5.1f  ->  %5.1f / %5.1f dB\n", at(oneI, 160), at(oneI, 360), at(oneT, 160), at(oneT, 360));
    std::printf("    erster + zweiter Trim:    %5.1f / %5.1f  ->  %5.1f / %5.1f dB\n", at(twoI, 160), at(twoI, 360), at(twoT, 160), at(twoT, 360));
    check(std::fabs(at(idleT, 160) - at(idleI, 160)) < 1.5, "entspannt: kein Flattern");
    check(at(oneT, 160) > at(oneI, 160) + 6, "ein heißer Trim: Kick klingt deutlich länger nach");
    check(at(twoT, 360) > -3 && at(twoI, 360) < -20, "zwei heiße Trims: schwingt bis zum nächsten Schlag durch (ideal nicht)");
    // pitch of the standing tone
    const size_t a = size_t(3.6 * FS) + 36 + size_t(0.2 * FS);
    std::vector<double> zc;
    for (size_t i = a; i < a + size_t(0.18 * FS); i++) if (twoT[i - 1] < 0 && twoT[i] >= 0) zc.push_back(double(i));
    const double period = zc.size() > 2 ? (zc.back() - zc.front()) / double(zc.size() - 1) / FS * 1000 : 0;
    std::printf("  stehender Ton: Periode %.1f ms = %.1f Hz (Aufnahme: 18.4 ms / 54.3 Hz)\n", period, 1000 / period);
    check(period > 17.5 && period < 19.5, "Ton bei ~54 Hz wie am Pult");
    double tail = 0;
    for (size_t i = size_t(4.9 * FS); i < size_t(5.2 * FS); i++) tail = std::max(tail, double(std::fabs(twoT[i])));
    std::printf("  nach dem letzten Schlag (Eingang still, 0.9..1.2 s danach): Spitze %.4f\n", tail);
    check(tail < 0.05, "ohne Eingang flattert es aus (keine Selbstoszillation)");
    // 8 strips with raised trims: nothing may keep running after the input stops
    for (int k = 0; k < 2; k++) {
      Run r8x;
      r8x.e.setParam(kParamSupply, k == 0 ? 0.5 : 1.0);
      r8x.e.prepare(FS, 512);
      r8x.e.setParam(kParamStrips, normOfStrips(8));
      for (int s = 0; s < 8; s++) r8x.e.setParam(stripParamId(s, kTrim), 0.4);
      std::vector<float> xi(size_t(FS * 3), 0.0f), yi, yr;
      for (int i = 0; i < 2000; i++) xi[size_t(i)] = float(0.3 * std::sin(2 * kPi * 54 * i / FS));
      r8x.process(xi, xi, yi, yr);
      double late = 0;
      for (size_t i = size_t(2.5 * FS); i < xi.size(); i++) late = std::max(late, double(std::fabs(yi[i])));
      std::printf("  8 Kanäle (Trim je +7.5 dB), Netzteil x%.0f: nach 2.5 s Spitze %.4f\n", supplyOfNorm(k == 0 ? 0.5 : 1.0), late);
      check(late < 0.01, "8 Kanäle: keine Selbstoszillation nach dem Signal");
    }
    for (size_t i = 0; i < size_t(4.0 * FS); i++) if (twoT[i] != twoT[i]) tail = 1;
    if (!outDir.empty()) {
      writeWav(outDir + "/flattern_ideal_zwei_trims.wav", twoI, twoI);
      writeWav(outDir + "/flattern_typisch_ein_trim.wav", oneT, oneT);
      writeWav(outDir + "/flattern_typisch_zwei_trims.wav", twoT, twoT);
    }
  }

  std::printf("Automation (8 Kanäle, 8x, zufällige Regler je Block):\n");
  {
    Run r(8);
    r.e.setParam(kParamStrips, 1.0);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(0, 1);
    std::vector<float> x = kick(5.0, 1.0), y(x.size()), yr(x.size());
    bool finite = true;
    double peak = 0;
    for (size_t p = 0; p < x.size(); p += 256) {
      for (int j = 0; j < 6; j++) r.e.setParam(stripParamId(int(u(rng) * 8), int(u(rng) * kSlots)), u(rng));
      const int n = int(std::min<size_t>(256, x.size() - p));
      const float* in[2] = { x.data() + p, x.data() + p };
      float* out[2] = { y.data() + p, yr.data() + p };
      r.e.process(in, out, 2, n);
      for (int i = 0; i < n; i++) { if (!std::isfinite(y[p + size_t(i)])) finite = false; peak = std::max(peak, double(std::fabs(y[p + size_t(i)]))); }
    }
    std::printf("  Spitze %.3f\n", peak);
    check(finite, "keine NaN/Inf bei wilder Automation");
  }

  std::printf("Rechenzeit (10 s Stereo, 48 kHz):\n");
  for (int os : { 4, 8 }) for (int strips : { 1, 8 }) {
    Run r(os);
    r.e.setParam(kParamStrips, normOfStrips(strips));
    std::vector<float> x = kick(10.0, 0.8), y, yr;
    const auto t0 = std::chrono::steady_clock::now();
    r.process(x, x, y, yr);
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %dx, %d Kanal/Kanäle: %.2f s Rechenzeit = %.1f %% eines Kerns\n", os, strips, s, s / 10 * 100);
    if (os == 4 && strips == 8) check(s < 5.0, "8 Kanäle bei 4x unter 50 % eines Kerns");
  }

  if (!outDir.empty()) {
    std::printf("Hörbeispiele:\n");
    writeWav(outDir + "/kick_dry.wav", k, k);
    writeWav(outDir + "/kick_1ch_lo15_fader10.wav", y1, y1r);
    writeWav(outDir + "/kick_4ch_chain.wav", y4, y4r);
  }
  std::printf(failures ? "FEHLER: %d Prüfungen\n" : "alle Prüfungen ok\n", failures);
  return failures ? 1 : 0;
}
