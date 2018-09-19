#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <Headers/kern_iokit.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/pci/IOPCIDevice.h>
#pragma clang diagnostic pop

class IOThunderboltController;
class IOThunderboltConnectionManager;
class IOThunderboltTransmitCommand;
class IOThunderboltReceiveCommand;

class TBE {
public:
  void init();
  void deinit();

  void onPatcherLoaded(KernelPatcher&);

private:
  static TBE* callbackInst;
  void patchKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

  static bool wrapNHIStart(void* that, IOService*);
  static uint32_t wrapHalRegisterRead32(void*, uint32_t);
  static void wrapHalRegisterWrite32(void*, uint32_t, uint32_t);
  static IOThunderboltConnectionManager* wrapConnectionManagerWithController(IOThunderboltController*);
  static int wrapSubmitTxCommandToNHI(void*, IOThunderboltTransmitCommand*);
  static int wrapRxCommandCallback(void*, void*, unsigned int, IOThunderboltReceiveCommand*);

  mach_vm_address_t origNHIStart { 0 };
  mach_vm_address_t origConnectionManagerWithController { 0 };
  mach_vm_address_t origSubmitTxCommandToNHI { 0 };
  mach_vm_address_t origRxCommandCallback { 0 };

  mach_vm_address_t halRegisterRead32_addr { 0 };
  mach_vm_address_t halRegisterWrite32_addr { 0 };
  mach_vm_address_t halGetParentBridgeDevice_addr { 0 };
  mach_vm_address_t halGetRootBridgeDevice_addr { 0 };
  mach_vm_address_t halGetRootPortDevice_addr { 0 };

  int halRegisterRead32(void* tbHAL, uint32_t address) { return reinterpret_cast<int(*)(void*, uint32_t)>(halRegisterRead32_addr)(tbHAL, address); }
  void halRegisterWrite32(void* tbHAL, uint32_t address, uint32_t value) { reinterpret_cast<void(*)(void*, uint32_t, uint32_t)>(halRegisterWrite32_addr)(tbHAL, address, value); }
  IOPCIDevice* halGetParentBridgeDevice(void* tbHAL) { return reinterpret_cast<IOPCIDevice*(*)(void*)>(halGetParentBridgeDevice_addr)(tbHAL); }
  IOPCIDevice* halGetRootBridgeDevice(void* tbHAL) { return reinterpret_cast<IOPCIDevice*(*)(void*)>(halGetRootBridgeDevice_addr)(tbHAL); }
  IOPCIDevice* halGetRootPortDevice(void* tbHAL) { return reinterpret_cast<IOPCIDevice*(*)(void*)>(halGetRootPortDevice_addr)(tbHAL); }

  bool nhi_mailbox_cmd(void* thunderboltHAL, uint32_t cmd, uint32_t data);
};
