#include "ids.h"
#include "processor.h"
#include "controller.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

BEGIN_FACTORY_DEF(eightbus::kVendor, "", "")

  DEF_CLASS2(INLINE_UID_FROM_FUID(eightbus::ProcessorUID), PClassInfo::kManyInstances, kVstAudioEffectClass,
             eightbus::kPluginName, 0, PlugType::kFxEQ, eightbus::kVersion, kVstVersionString,
             eightbus::Processor::createInstance)

  DEF_CLASS2(INLINE_UID_FROM_FUID(eightbus::ControllerUID), PClassInfo::kManyInstances, kVstComponentControllerClass,
             "8-Bus Controller", 0, "", eightbus::kVersion, kVstVersionString,
             eightbus::Controller::createInstance)

END_FACTORY
