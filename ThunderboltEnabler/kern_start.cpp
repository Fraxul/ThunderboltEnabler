#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>

#include "kern_tbe.hpp"

static TBE tbe;

static const char *bootargOff[] {
  "-tbeoff"
};

static const char *bootargDebug[] {
  "tbedbg"
};

static const char *bootargBeta[] {
  "-tbebeta"
};

PluginConfiguration ADDPR(config) {
  xStringify(PRODUCT_NAME),
  parseModuleVersion(xStringify(MODULE_VERSION)),
  LiluAPI::AllowNormal | LiluAPI::AllowInstallerRecovery,
  bootargOff,
  arrsize(bootargOff),
  bootargDebug,
  arrsize(bootargDebug),
  bootargBeta,
  arrsize(bootargBeta),
  KernelVersion::HighSierra,
  KernelVersion::Mojave,
  []() {
    tbe.init();
  }
};


