// Processor -> editor exchange (meters, OL LEDs). Processor and controller live in the same process
// (the plugin is not "distributable"); the processor hands the controller a pointer once (IMessage).
// The audio thread publishes ~60x per second with try_lock (never blocks); the editor reads in its timer.
// For the spectrum analyzer the audio thread also writes input and output (mono sum) into a lock-free ring.
#pragma once
#include "engine.h"
#include <atomic>
#include <mutex>

namespace eightbus {

class UiBridge {
public:
  void addRef() { refs.fetch_add(1); }
  void release() { if (refs.fetch_sub(1) == 1) delete this; }

  // audio thread: peaks accumulate until the editor has read them
  void publish(const r8::Meters& m) {
    if (!mtx.try_lock()) return;
    for (int c = 0; c < 2; c++) { snap.in[c] = std::max(snap.in[c], m.in[c]); snap.out[c] = std::max(snap.out[c], m.out[c]); }
    for (int s = 0; s < r8::kMaxStrips; s++) snap.ol[s] = std::max(snap.ol[s], m.ol[s]);
    snap.strips = m.strips;
    seq++;
    mtx.unlock();
  }
  // editor thread
  bool read(r8::Meters& out) {
    std::lock_guard<std::mutex> l(mtx);
    if (seq == lastRead) return false;
    lastRead = seq;
    out = snap;
    snap = r8::Meters();
    snap.strips = out.strips;
    return true;
  }

  // audio thread: one sample of input and output (mono)
  void pushAudio(float in, float out) {
    const uint32_t h = head.load(std::memory_order_relaxed);
    if (h - tail.load(std::memory_order_acquire) >= Ring) return;     // editor closed or too slow: drop
    ringIn[h % Ring] = in;
    ringOut[h % Ring] = out;
    head.store(h + 1, std::memory_order_release);
  }
  void setSampleRate(double fs) { rate.store(fs); }
  // editor thread
  int readAudio(float* in, float* out, int max) {
    uint32_t t = tail.load(std::memory_order_relaxed);
    const uint32_t h = head.load(std::memory_order_acquire);
    int n = 0;
    while (t != h && n < max) { in[n] = ringIn[t % Ring]; out[n] = ringOut[t % Ring]; t++; n++; }
    tail.store(t, std::memory_order_release);
    return n;
  }
  double sampleRate() const { return rate.load(); }

private:
  static constexpr uint32_t Ring = 1 << 15;
  float ringIn[Ring] = {}, ringOut[Ring] = {};
  std::atomic<uint32_t> head{ 0 }, tail{ 0 };
  std::atomic<double> rate{ 48000 };
  std::atomic<int> refs{ 1 };
  std::mutex mtx;
  r8::Meters snap;
  uint32_t seq = 0, lastRead = 0;
};

} // namespace eightbus
