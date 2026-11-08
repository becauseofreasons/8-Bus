#pragma once
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "bridge.h"
#include "engine.h"
#include <memory>
#include <mutex>
#include <vector>

namespace eightbus {

class Processor : public Steinberg::Vst::AudioEffect {
public:
  Processor();
  ~Processor() override;
  static Steinberg::FUnknown* createInstance(void*) { return (Steinberg::Vst::IAudioProcessor*)new Processor; }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API connect(Steinberg::Vst::IConnectionPoint* other) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
                                                   Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;
  Steinberg::uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE;
  Steinberg::uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) SMTG_OVERRIDE;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) SMTG_OVERRIDE;

private:
  r8::Engine engine;
  UiBridge* bridge = nullptr;
  double sampleRate = 48000;
  int64_t uiCountdown = 0;
  // parameter values as last seen (host thread reads them for getState)
  std::mutex mtx;
  std::vector<double> values;            // indexed like r8::paramDefs()
  std::vector<double> pending;           // from setState, applied on the audio thread
  bool hasPending = false;
  uint32_t seed = 1;

  void applyParam(Steinberg::Vst::ParamID id, double v);
};

} // namespace eightbus
