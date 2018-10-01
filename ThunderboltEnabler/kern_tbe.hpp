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
  void patchKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size);

private:
  static TBE* callbackInst;

  static IOThunderboltConnectionManager* connectionManagerWithController(IOThunderboltController*);
  static int wrapSubmitTxCommandToNHI(void*, IOThunderboltTransmitCommand*);
  static int wrapRxCommandCallback(void*, void*, unsigned int, IOThunderboltReceiveCommand*);

  mach_vm_address_t origSubmitTxCommandToNHI { 0 };
  mach_vm_address_t origRxCommandCallback { 0 };
};

