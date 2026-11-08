// End-to-end test of the built plugin: loads 8-Bus.vst3 like a DAW does, sends audio and
// parameter changes, checks level, latency, clipping, the read-outs and the saved state.
//   r8_hosttest <8-Bus.vst3> [out.wav]
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

static const double FS = 48000;
static const int BLOCK = 256;
static int failures = 0;
static void check(bool ok, const char* what) { std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) failures++; }

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "r8_hosttest <8-Bus.vst3>\n"); return 2; }
  std::string err;
  auto module = VST3::Hosting::Module::create(argv[1], err);
  if (!module) { std::fprintf(stderr, "Modul nicht geladen: %s\n", err.c_str()); return 1; }
  HostApplication host;
  PluginContextFactory::instance().setPluginContext(&host);
  auto factory = module->getFactory();
  VST3::Hosting::ClassInfo info;
  bool found = false;
  for (auto& ci : factory.classInfos()) if (ci.category() == kVstAudioEffectClass) { info = ci; found = true; break; }
  if (!found) { std::fprintf(stderr, "keine Audio-Klasse\n"); return 1; }
  std::printf("%s (%s, %s) %s\n", info.name().c_str(), info.vendor().c_str(), info.subCategoriesString().c_str(), info.version().c_str());

  auto provider = owned(new PlugProvider(factory, info, true));
  if (!provider->initialize()) { std::fprintf(stderr, "PlugProvider fehlgeschlagen\n"); return 1; }
  IPtr<IComponent> comp = provider->getComponentPtr();
  IPtr<IEditController> ctrl = provider->getControllerPtr();
  FUnknownPtr<IAudioProcessor> proc(comp);
  check(comp && ctrl && proc, "Komponente, Controller und Prozessor geladen");
  {
    MemoryStream st;
    comp->getState(&st);
    st.seek(0, IBStream::kIBSeekSet, nullptr);
    ctrl->setComponentState(&st);
  }
  const int32 nParams = ctrl->getParameterCount();
  std::printf("  %d Parameter\n", nParams);
  check(nParams == 7 + 8 * 11, "7 globale + 8 x 11 Kanal-Parameter");

  // read-outs as the DAW shows them
  auto show = [&](ParamID id, double v) {
    String128 s;
    ctrl->getParamStringByValue(id, v, s);
    char a[128];
    UString(s, 128).toAscii(a, 128);
    return std::string(a);
  };
  std::printf("  Anzeige: Hi Mid Freq 0.5 = %s, Lo Mid Freq 0 = %s, BW 1 = %s, Fader 0.75 = %s, Trim default = %s\n",
              show(102, 0.5).c_str(), show(105, 0.0).c_str(), show(103, 1.0).c_str(), show(108, 0.75).c_str(),
              show(100, ctrl->getParamNormalized(100)).c_str());
  check(show(108, 0.75) == "0.0", "Fader-Mitte U = 0 dB");

  ProcessSetup setup{ kRealtime, kSample32, BLOCK, FS };
  check(proc->setupProcessing(setup) == kResultOk, "setupProcessing");
  comp->activateBus(kAudio, kInput, 0, true);
  comp->activateBus(kAudio, kOutput, 0, true);
  comp->setActive(true);
  proc->setProcessing(true);
  check(proc->getLatencySamples() == 36, "Latenz 36 Samples gemeldet");

  HostProcessData data;
  data.prepare(*comp, BLOCK, kSample32);
  ParameterChanges inCh(64);
  data.inputParameterChanges = &inCh;
  data.numSamples = BLOCK;

  auto run = [&](double seconds, double amp, double hz, std::vector<float>& out, std::vector<std::pair<ParamID, double>> params = {}) {
    out.clear();
    const int64_t n = int64_t(seconds * FS);
    for (int64_t pos = 0; pos < n; pos += BLOCK) {
      inCh.clearQueue();
      if (pos == 0)
        for (auto& p : params) { int32 idx; if (auto* q = inCh.addParameterData(p.first, idx)) q->addPoint(0, p.second, idx); ctrl->setParamNormalized(p.first, p.second); }
      for (int c = 0; c < 2; c++)
        for (int i = 0; i < BLOCK; i++) data.inputs[0].channelBuffers32[c][i] = float(amp * std::sin(2 * M_PI * hz * double(pos + i) / FS));
      proc->process(data);
      for (int i = 0; i < BLOCK; i++) out.push_back(data.outputs[0].channelBuffers32[0][i]);
    }
  };
  auto rms = [](const std::vector<float>& x, size_t a, size_t b) { double q = 0; for (size_t i = a; i < b; i++) q += double(x[i]) * x[i]; return std::sqrt(q / double(b - a)); };
  auto peak = [](const std::vector<float>& x, size_t a) { double p = 0; for (size_t i = a; i < x.size(); i++) p = std::max(p, double(std::fabs(x[i]))); return p; };

  std::printf("Klang:\n");
  std::vector<float> out;
  run(1.0, 0.1, 1000, out);
  const double g0 = 20 * std::log10(rms(out, 24000, 48000) / (0.1 / std::sqrt(2.0)));
  std::printf("  neutral, 1 kHz -20 dBFS: %+.2f dB\n", g0);
  check(std::fabs(g0) < 0.3, "neutral = 0 dB");
  run(1.0, 0.1, 3000, out, { { 101, 1.0 } });          // channel 1 HI MID +15 at 3 kHz (freq knob at 0.5 = 2.7 kHz)
  const double g1 = 20 * std::log10(rms(out, 24000, 48000) / (0.1 / std::sqrt(2.0)));
  std::printf("  Hi Mid voll, 3 kHz: %+.2f dB\n", g1);
  check(g1 > 12, "Hi Mid hebt an");
  run(1.0, 1.0, 60, out, { { 101, 0.5 }, { 107, 1.0 }, { 108, 1.0 } });   // LO +15, fader +10, full level
  const double p = peak(out, 24000);
  std::printf("  60 Hz 0 dBFS, LO +15, Fader +10: Spitze %.3f\n", p);
  check(p > 1.5 && p < 2.4, "Op-Amp-Begrenzung (Schiene ca. +5.4 dBFS, plus Verschiebung durch die Koppelkondensatoren)");
  run(0.5, 1.0, 60, out, { { 6, 1.0 } });                             // bypass
  std::printf("  Bypass: Spitze %.3f\n", peak(out, 12000));
  check(std::fabs(peak(out, 12000) - 1.0) < 0.01, "Bypass = Eingang");

  std::printf("Zustand:\n");
  MemoryStream st;
  check(comp->getState(&st) == kResultOk, "getState");
  int64 size = 0;
  st.tell(&size);
  check(size == 16 + (7 + 88) * 12, "Größe (Kopf + 95 Parameter)");
  st.seek(0, IBStream::kIBSeekSet, nullptr);
  check(comp->setState(&st) == kResultOk, "setState");
  st.seek(0, IBStream::kIBSeekSet, nullptr);
  check(ctrl->setComponentState(&st) == kResultOk, "Controller liest den Zustand");
  check(std::fabs(ctrl->getParamNormalized(107) - 1.0) < 1e-9, "Wert LO kommt zurück");

  proc->setProcessing(false);
  comp->setActive(false);
  data.unprepare();
  provider = nullptr;
  PluginContextFactory::instance().setPluginContext(nullptr);
  std::printf(failures ? "FEHLER: %d Prüfungen\n" : "alle Prüfungen ok\n", failures);
  return failures ? 1 : 0;
}
