#include "processor.h"
#include "ids.h"
#include "state.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include <algorithm>
#include <random>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace eightbus {

Processor::Processor() {
  setControllerClass(ControllerUID);
  const auto& defs = r8::paramDefs();
  values.resize(defs.size());
  for (size_t i = 0; i < defs.size(); i++) values[i] = defs[i].def;
  std::random_device rd;
  seed = rd() | 1u;                          // every new instance: its own component tolerances
  engine.setSeed(seed);
  bridge = new UiBridge;
}

Processor::~Processor() {
  if (bridge) bridge->release();
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  tresult r = AudioEffect::initialize(context);
  if (r != kResultOk) return r;
  addAudioInput(STR16("In"), SpeakerArr::kStereo);
  addAudioOutput(STR16("Out"), SpeakerArr::kStereo);
  return kResultOk;
}

// hand the bridge to the controller (same process)
tresult PLUGIN_API Processor::connect(IConnectionPoint* other) {
  tresult r = AudioEffect::connect(other);
  if (r == kResultOk && bridge) {
    if (auto msg = owned(allocateMessage())) {
      msg->setMessageID("bridge");
      msg->getAttributes()->setInt("ptr", int64(reinterpret_cast<intptr_t>(bridge)));
      sendMessage(msg);
    }
  }
  return r;
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) {
  if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0] &&
      (outputs[0] == SpeakerArr::kStereo || outputs[0] == SpeakerArr::kMono))
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  return kResultFalse;
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
  return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
  // not processing now: safe to allocate and to touch the engine
  std::lock_guard<std::mutex> lock(mtx);
  sampleRate = setup.sampleRate > 0 ? setup.sampleRate : 48000;
  if (hasPending) {
    for (size_t i = 0; i < pending.size(); i++) values[i] = pending[i];
    hasPending = false;
  }
  for (size_t i = 0; i < values.size(); i++) engine.setParam(r8::paramDefs()[i].id, values[i]);
  engine.setSeed(seed);
  engine.prepare(sampleRate, std::max<int32>(setup.maxSamplesPerBlock, 32));
  if (bridge) bridge->setSampleRate(sampleRate);
  return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (state) engine.reset();
  return AudioEffect::setActive(state);
}

uint32 PLUGIN_API Processor::getLatencySamples() { return uint32(engine.latency()); }
uint32 PLUGIN_API Processor::getTailSamples() { return uint32(sampleRate * 0.25); }

void Processor::applyParam(ParamID id, double v) {
  const int i = r8::paramIndex(id);
  if (i < 0) return;
  engine.setParam(id, v);
  values[size_t(i)] = v;        // a torn read in getState is harmless (one value, next save fixes it)
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
  // state loaded by the host thread
  if (mtx.try_lock()) {
    if (hasPending) {
      engine.setSeed(seed);
      for (size_t i = 0; i < pending.size(); i++) applyParam(r8::paramDefs()[i].id, pending[i]);
      hasPending = false;
    }
    mtx.unlock();
  }
  if (IParameterChanges* changes = data.inputParameterChanges) {
    const int32 n = changes->getParameterCount();
    for (int32 i = 0; i < n; i++) {
      IParamValueQueue* q = changes->getParameterData(i);
      if (!q || q->getPointCount() <= 0) continue;
      int32 offset;
      ParamValue v;
      if (q->getPoint(q->getPointCount() - 1, offset, v) == kResultTrue) applyParam(q->getParameterId(), v);
    }
  }
  if (data.numSamples <= 0 || data.numInputs < 1 || data.numOutputs < 1) return kResultOk;

  AudioBusBuffers& in = data.inputs[0];
  AudioBusBuffers& out = data.outputs[0];
  const int nch = std::min(in.numChannels, out.numChannels);
  if (nch <= 0 || !in.channelBuffers32 || !out.channelBuffers32) return kResultOk;
  // analyzer: input before processing (the host may process in place), output after
  float inMono[8192];
  const int na = std::min<int32>(data.numSamples, 8192);
  for (int i = 0; i < na; i++) {
    float v = 0;
    for (int c = 0; c < nch; c++) v += in.channelBuffers32[c][i];
    inMono[i] = v / float(nch);
  }
  engine.process(in.channelBuffers32, out.channelBuffers32, nch, data.numSamples);
  if (bridge)
    for (int i = 0; i < na; i++) {
      float v = 0;
      for (int c = 0; c < nch; c++) v += out.channelBuffers32[c][i];
      bridge->pushAudio(inMono[i], v / float(nch));
    }
  for (int32 c = nch; c < out.numChannels; c++)
    if (out.channelBuffers32[c]) std::fill(out.channelBuffers32[c], out.channelBuffers32[c] + data.numSamples, 0.0f);
  out.silenceFlags = 0;

  uiCountdown -= data.numSamples;
  if (bridge && uiCountdown <= 0) {
    uiCountdown = int64_t(sampleRate / 60);
    r8::Meters m;
    engine.takeMeters(m);
    bridge->publish(m);
  }
  return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* stream) {
  PluginState st;
  if (!st.read(stream)) return kResultFalse;
  std::lock_guard<std::mutex> lock(mtx);
  pending = values;
  for (const auto& p : st.params) {
    const int i = r8::paramIndex(p.first);
    if (i >= 0) pending[size_t(i)] = p.second;
  }
  seed = st.seed ? st.seed : seed;
  hasPending = true;
  return kResultOk;
}

tresult PLUGIN_API Processor::getState(IBStream* stream) {
  PluginState st;
  {
    std::lock_guard<std::mutex> lock(mtx);
    const std::vector<double>& src = hasPending ? pending : values;
    st.seed = seed;
    for (size_t i = 0; i < src.size(); i++) st.params.push_back({ r8::paramDefs()[i].id, src[i] });
  }
  return st.write(stream) ? kResultOk : kResultFalse;
}

} // namespace eightbus
