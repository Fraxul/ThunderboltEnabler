#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pci/IOPCIDevice.h>
#pragma clang diagnostic pop

class WMI;

class ThunderboltEnabler : public IOService {
  OSDeclareDefaultStructors(ThunderboltEnabler);
  typedef IOService super;

public:
  virtual IOService* probe(IOService* provider, SInt32* score) override;
  virtual bool start(IOService* provider) override;
  virtual void stop(IOService* provider) override;

  void doPowerOn();
  static IOReturn pciDeviceConfigHandler(void* refThis, IOMessage message, IOPCIDevice*, uint32_t state);

  IOReturn pciDeviceConfigHandler(IOMessage message, IOPCIDevice*, uint32_t state);
  void setChainConfigHandler(IOPCIDeviceConfigHandler, void* ref);

protected:
  WMI* m_wmi;
  IOPCIDeviceConfigHandler m_chainConfigHandler;
  void* m_chainConfigHandlerRef;
};


