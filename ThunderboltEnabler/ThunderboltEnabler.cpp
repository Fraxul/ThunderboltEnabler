#include "ThunderboltEnabler.h"
#include "WMI.h"

#define INTEL_WMI_THUNDERBOLT_GUID "86ccfd48-205e-4a77-9c48-2021cbede341"

OSDefineMetaClassAndStructors(ThunderboltEnabler, IOService);

IOService* ThunderboltEnabler::probe(IOService* provider, SInt32* score) {
  if (!super::probe(provider, score))
    return NULL;

  return this;
}

bool ThunderboltEnabler::start(IOService* provider) {
  if (!super::start(provider))
    return false;

  m_wmi = new WMI(provider);
  if (!m_wmi->initialize())
    return false;

  if (!m_wmi->hasMethod(INTEL_WMI_THUNDERBOLT_GUID)) {
    kprintf("ThunderboltEnabler::start(): WMI object does not expose a method for the Intel Thunderbolt WMI GUID (%s)\n", INTEL_WMI_THUNDERBOLT_GUID);
    return false;
  }

  doPowerOn();

  registerService();
  return true;
}

/*static*/ int ThunderboltEnabler::pciDeviceConfigHandler(void* refThis, IOMessage message, IOPCIDevice* device, uint32_t state) {
  return reinterpret_cast<ThunderboltEnabler*>(refThis)->pciDeviceConfigHandler(message, device, state);
}

int ThunderboltEnabler::pciDeviceConfigHandler(IOMessage message, IOPCIDevice* device, uint32_t state) {
  //kprintf("ThunderboltEnabler::pciDeviceConfigHandler(0x%x, %p, %u)\n", message, device, state);
  if (message == kIOMessageDeviceWillPowerOn) {
    doPowerOn();
  }

  if (m_chainConfigHandler) {
    return m_chainConfigHandler(m_chainConfigHandlerRef, message, device, state);
  }
  return kIOReturnSuccess;
}

void ThunderboltEnabler::setChainConfigHandler(IOPCIDeviceConfigHandler handler, void* ref) {
  m_chainConfigHandler = handler;
  m_chainConfigHandlerRef = ref;
}


void ThunderboltEnabler::doPowerOn() {
  OSObject* params[3] = {
      OSNumber::withNumber(0ULL, 32),
      OSNumber::withNumber(0ULL, 32),
      OSNumber::withNumber(1ULL, 32)
  };

  if (m_wmi->executeMethod(INTEL_WMI_THUNDERBOLT_GUID, NULL, params, 3)) {
    kprintf("ThunderboltEnabler::doPowerOn(): Power on via WMI.\n");
  } else {
    kprintf("ThunderboltEnabler::doPowerOn(): Couldn't power on via WMI.\n");
  }

  OSSafeReleaseNULL(params[0]);
  OSSafeReleaseNULL(params[1]);
  OSSafeReleaseNULL(params[2]);
}

void ThunderboltEnabler::stop(IOService* provider) {
  super::stop(provider);
  delete m_wmi;
  m_wmi = NULL;
}

