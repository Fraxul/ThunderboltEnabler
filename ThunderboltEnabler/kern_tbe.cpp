#include "kern_tbe.hpp"

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_disasm.hpp>
#include <Headers/kern_iokit.hpp>
#include "AppleThunderboltGenericHAL.h"
#include "ICMXDomainRegistry.h"
#include "IOThunderboltConfigICMCommand.h"
#include "IOThunderboltConfigReadCommand.h"
#include "IOThunderboltConnectionManager.h"
#include "IOThunderboltController.h"
#include "IOThunderboltControlPath.h"
#include "IOThunderboltICMListener.h"
#include "IOThunderboltLocalNode.h"
#include "IOThunderboltReceiveCommand.h"
#include "IOThunderboltTransmitCommand.h"
#include "ThunderboltEnabler.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <i386/proc_reg.h>
#include <libkern/libkern.h>
#include <mach/vm_map.h>

#include "tb_constants.h"
#include "iopcifamily_private.h"

#define LOG_PACKET_BYTES 0

TBE* TBE::callbackInst;

// Patch SystemUIServer so that the ExpressCard menu extra doesn't load. It incorrectly identifies Thunderbolt devices as ExpressCards.
// I doubt there's any overlap between machines with ExpressCard slots and machines with Thunderbolt controllers.
static const char* sysUIServerPath = "/System/Library/CoreServices/SystemUIServer.app/Contents/MacOS/SystemUIServer";
static const size_t sysUIServerPath_strlen = sizeof("/System/Library/CoreServices/SystemUIServer.app/Contents/MacOS/SystemUIServer") - 1;
static const char* binaryExpressCardMenuExtra = "/System/Library/CoreServices/Menu Extras/ExpressCard.menu/Contents/MacOS/ExpressCard";
// This patch changes the jump to skip enumerating the controllers if the count is zero, instead of if it's nonzero.
// This should ensure that the controller count always remains at zero.
static const uint8_t expressCardEnumDisablePatchFind[] = {
  0x8B, 0x05, 0x10, 0x15, 0x00, 0x00,    // mov        eax, dword [gControllerCount]
  0x85, 0xC0,                            // test       eax, eax
  0x0F, 0x85, // 0xE2, 0x00, 0x00, 0x00  // jne        loc_24a2 (location offset omitted from match)
};
static const uint8_t expressCardEnumDisablePatchRepl[] = {
  0x8B, 0x05, 0x10, 0x15, 0x00, 0x00,    // mov        eax, dword [gControllerCount]
  0x85, 0xC0,                            // test       eax, eax
  0x0F, 0x84, // 0xE2, 0x00, 0x00, 0x00  // je         loc_24a2 (location offset omitted from match)
};
static UserPatcher::BinaryModPatch expressCardEnumDisablePatch {
  CPU_TYPE_X86_64,
  expressCardEnumDisablePatchFind,
  expressCardEnumDisablePatchRepl,
  arrsize(expressCardEnumDisablePatchFind),
  0, // skip 0 -> replace all occurances
  1, // count 1
  UserPatcher::FileSegment::SegmentTextText,
  1
};
static UserPatcher::ProcInfo procSysUIserver { sysUIServerPath, sysUIServerPath_strlen, 1 };
static UserPatcher::BinaryModInfo expressCardMenuExtraPatch { binaryExpressCardMenuExtra, &expressCardEnumDisablePatch, 1};

static const char* pathAppleThunderboltIP[] {
  "/System/Library/Extensions/AppleThunderboltIP.kext/Contents/MacOS/AppleThunderboltIP",
};
static KernelPatcher::KextInfo kexts[] {
  {"com.apple.driver.AppleThunderboltIP", pathAppleThunderboltIP, arrsize(pathAppleThunderboltIP), {true}, {}, KernelPatcher::KextInfo::Unloaded},
};


int XDomainUUIDRequestCommand_submit(void* that);
int XDomainUUIDRequestCommand_submitSynchronous(void* that);
int ControlPath_sleep(void* that);
int ControlPath_wake(void* that);
void IOPCIConfigurator_bridgeScanBus(void* that, IOPCIConfigEntry*, uint8_t, uint32_t);
int32_t IOPCIConfigurator_scanProc(void* that, void* ref, IOPCIConfigEntry*);
static void(*IOPCIConfigurator_bridgeScanBus_orig)(void*, IOPCIConfigEntry*, uint8_t, uint32_t) = nullptr;
static int32_t(*IOPCIConfigurator_scanProc_orig)(void*, void*, IOPCIConfigEntry*) = nullptr;

bool ThunderboltIPService_start(void*, IOService*);
static bool(*ThunderboltIPService_start_orig)(void*, IOService*) = nullptr;

// Linker stubs for function and vtable patching
extern "C" {
  // IOThunderboltFamily.kext
  void* _ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController(void*);
  int _ZN24IOThunderboltControlPath17rxCommandCallbackEPviP27IOThunderboltReceiveCommand(void*, void*, unsigned int, void*);

  extern size_t _ZTV44IOThunderboltConfigXDomainUUIDRequestCommand;
  int _ZN26IOThunderboltConfigCommand6submitEv(void*);
  int _ZN26IOThunderboltConfigCommand17submitSynchronousEv(void*);

  int _ZN44IOThunderboltConfigXDomainUUIDRequestCommand8completeEi(void*, int);

  extern size_t _ZTV24IOThunderboltControlPath;
  int _ZN24IOThunderboltControlPath5sleepEv(void*);
  int _ZN24IOThunderboltControlPath4wakeEv(void*);

  // AppleThunderboltNHI.kext
  int _ZN31AppleThunderboltNHITransmitRing18submitCommandToNHIEP28IOThunderboltTransmitCommand(void*, IOThunderboltTransmitCommand*);

  // IOPCIFamily.kext
  extern const OSSymbol* gIOPCITunnelledKey;
  extern void _ZN17IOPCIConfigurator16bridgeProbeChildEP16IOPCIConfigEntry17IOPCIAddressSpace(void*, void*, uint32_t, uint32_t);
  extern IOReturn _ZN15IOPCI2PCIBridge9checkLinkEj(void*, uint32_t);
  extern void _ZN17IOPCIConfigurator13bridgeScanBusEP16IOPCIConfigEntryh(void*, IOPCIConfigEntry*, uint8_t, uint32_t);
  extern void _ZN17IOPCIConfigurator8scanProcEPvP16IOPCIConfigEntry(void*, void*, IOPCIConfigEntry*);
}

const uint8_t IOPCIConfiguratorBridgeProbeChildLinkInterruptPatchFind[] = {
  // This corresponds to the condition in these lines of code from IOPCIFamily-320.30.2 - IOPCIConfigurator.cpp:1842
  //    if ((kPCIHotPlugTunnel == (kPCIHPTypeMask & bridge->supportsHotPlug))     <<<<====
  //      && (0x60 == (0xf0 & expressCaps))) // downstream port                   <<<<====
  //    {
  //      if ((kLinkCapDataLinkLayerActiveReportingCapable & linkCaps)
  //       && (kSlotCapHotplug & slotCaps))
  //      {
  //        child->linkInterrupts = true;
  //      }
  //    }
  // We ungate the hotplug type mask and downstream port requirements and set the linkInterrupts value based only on the
  // reported link and express caps bits. This allows the root port for the thunderbolt controller to be correctly marked as
  // hotplug + link-interrupt capable and lets the DSBs enumerate after force-power-on without having to inject any DT properties.

  // Just NOP out all of these following instructions:
  //0x81, 0xE3, 0xF0, 0x00, 0x00, 0x00, // and     ebx, 0F0h
  //0x83, 0xFB, 0x60,                   // cmp     ebx, 60h
  0x75, 0x19,                         // jnz     short loc_17037
  0x48, 0x8B, 0x45, 0xC8,             // mov     rax, [rbp+var_38]
  0x8A, 0x80, 0x21, 0x01, 0x00, 0x00, // mov     al, [rax+121h]
  0x24, 0xF0,                         // and     al, 0F0h
  0x3C, 0x30,                         // cmp     al, 30h
  0x75, 0x09,                         // jnz     short loc_17037
};

static size_t* scanVtableForFunction(size_t* vtable_base, void* function) {
  // Skip over the first couple of zero entries. There are typically two zero-slots in IOKit vtables.
  while ((*vtable_base) == 0) ++vtable_base;

  // Scan for a matching entry, stopping if we encounter a zero-slot.
  for (; *vtable_base; ++vtable_base) {
    if (*vtable_base == reinterpret_cast<size_t>(function))
      return vtable_base;
  }
  return NULL;
}

static const uint8_t* memmem(const uint8_t* big, size_t big_length, const uint8_t* little, size_t little_length) {
  if (little_length > big_length)
    return NULL;

  const uint8_t* bigmax = big + (big_length - little_length);
  for (const uint8_t* bp = big; bp != bigmax; ++bp) {
    if (memcmp(bp, little, little_length) == 0)
      return bp;
  }

  return NULL;
}

static void TBE_onPatcherLoaded_callback(void* that, KernelPatcher& patcher) { reinterpret_cast<TBE*>(that)->onPatcherLoaded(patcher); }


static const size_t s_tempExecutableMemorySize = 4096;
static uint8_t s_tempExecutableMemory[s_tempExecutableMemorySize] __attribute__((section("__TEXT,__text")));
static size_t s_tempExecutableMemoryOffset = 0;

static size_t assembleIndirectAbsoluteJump(uint8_t* out, const void* target) {
  // jmp qword ptr [rip], then an 8-byte target address immediately following.
  out[0] = 0xff;
  out[1] = 0x25;
  out[2] = 0;
  out[3] = 0;
  out[4] = 0;
  out[5] = 0;
  lilu_os_memcpy(out + 6, &target, sizeof(uint64_t));
  return 14;
}

static void* routeFunctionWithTrampolineInternal(void* originalFunction, const void* targetFunction) {
  // Route originalFunction by overwriting its prologue with an absolute JMP to targetFunction. Generate a trampoline that
  // can be used to call originalFunction and return its address.

  uint8_t* trampolineStart = s_tempExecutableMemory + s_tempExecutableMemoryOffset;
  size_t trampolineLength = Disassembler::quickInstructionSize(reinterpret_cast<mach_vm_address_t>(originalFunction), 14);
  uint8_t* originalFunctionResumePoint = reinterpret_cast<uint8_t*>(originalFunction) + trampolineLength;

  // Assemble the trampoline: original prologue, followed by a jump to the resume point into the end of the trampoline.
  lilu_os_memcpy(trampolineStart, originalFunction, trampolineLength);
  s_tempExecutableMemoryOffset += trampolineLength + assembleIndirectAbsoluteJump(trampolineStart + trampolineLength, originalFunctionResumePoint);
  s_tempExecutableMemoryOffset = ((s_tempExecutableMemoryOffset + 15) & (~0xf)); // pad-align

  asm volatile("clflush (%0)" : : "r" (trampolineStart));
  asm volatile("clflush (%0)" : : "r" (trampolineStart + 16));
  asm volatile("mfence");

  // Assemble a jump to the route function into the beginning of the original function, overwriting the original prologue (now moved to the trampoline)
  assembleIndirectAbsoluteJump(reinterpret_cast<uint8_t*>(originalFunction), targetFunction);

  asm volatile("clflush (%0)" : : "r" (reinterpret_cast<uint8_t*>(originalFunction)));
  asm volatile("clflush (%0)" : : "r" (reinterpret_cast<uint8_t*>(originalFunction) + 16));
  asm volatile("mfence");

  return trampolineStart;
}


void TBE::init() {
  callbackInst = this;
  
  kprintf("ThunderboltEnabler: TBE::init()\n");
  ICMXDomainRegistry::staticInit();

  // We need to make some very early patches to IOThunderboltFamily and IOPCIFamily.
  // These need to be in place well before Lilu's patcher is ready, so we are unfortunately on our own here.

  kprintf("ThunderboltEnabler: Scanning vtable at %p\n", &_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand);
  // Compute offsets to patch in the IOThunderboltConfigXDomainUUIDRequestCommand vtable
  size_t* uuidreq_submit_offset = scanVtableForFunction(&_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand, reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand6submitEv));
  kprintf("ThunderboltEnabler: IOThunderboltConfigXDomainUUIDRequestCommand vtable contains IOThunderboltConfigCommand::submit (%p) at %p\n", reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand6submitEv), uuidreq_submit_offset);
  size_t* uuidreq_submitSynchronous_offset = scanVtableForFunction(&_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand, reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand17submitSynchronousEv));
  kprintf("ThunderboltEnabler: IOThunderboltConfigXDomainUUIDRequestCommand vtable contains IOThunderboltConfigCommand::submitSynchronous (%p) at %p\n", reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand17submitSynchronousEv), uuidreq_submitSynchronous_offset);

  // Compute offsets to patch in the IOThunderboltControlPath vtable
  size_t* controlPath_sleep_offset = scanVtableForFunction(&_ZTV24IOThunderboltControlPath, reinterpret_cast<void*>(&_ZN24IOThunderboltControlPath5sleepEv));
  size_t* controlPath_wake_offset = scanVtableForFunction(&_ZTV24IOThunderboltControlPath, reinterpret_cast<void*>(&_ZN24IOThunderboltControlPath4wakeEv));

  if (!(uuidreq_submit_offset && uuidreq_submitSynchronous_offset && controlPath_sleep_offset && controlPath_wake_offset)) {
    kprintf("ThunderboltEnabler: ERROR: Unable to find some vtable offsets to patch, bailing out.\n");
    return;
  }

  // Find offset to NOP out in IOPCIFamily'IOPCIConfigurator::bridgeProbeChild. The function is 0x64c bytes long in 10.13.6. We'll search up to 0x1000 bytes to be safe.
  uint8_t* bridgeProbeChild_functionStart = reinterpret_cast<uint8_t*>(&_ZN17IOPCIConfigurator16bridgeProbeChildEP16IOPCIConfigEntry17IOPCIAddressSpace);
  size_t bridgeProbeChild_scanLength = 0x1000;

  uint8_t* bridgeProbeChild_patchOffset = const_cast<uint8_t*>(memmem(bridgeProbeChild_functionStart, bridgeProbeChild_scanLength, IOPCIConfiguratorBridgeProbeChildLinkInterruptPatchFind, arrsize(IOPCIConfiguratorBridgeProbeChildLinkInterruptPatchFind)));
  if (!bridgeProbeChild_patchOffset) {
    kprintf("ThunderboltEnabler: ERROR: Unable to find patch offset in IOPCIFamily.kext IOPCIConfigurator::bridgeProbeChild\n");
    return;
  }

  // Disable interrupts while we mess around with CR0
  asm volatile("cli");

  // Enable writing to protected pages
  uintptr_t cr0 = get_cr0();
  if (cr0 & CR0_WP) {
    set_cr0(cr0 & (~CR0_WP));
  }

  // Replace IOThunderboltConnectionManager::withController() with our version. (We don't call the original so we don't bother generating a trampoline)
  assembleIndirectAbsoluteJump((uint8_t*) &_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController, (void*) &connectionManagerWithController);

  // Patch vtables
  *uuidreq_submit_offset = reinterpret_cast<size_t>(&XDomainUUIDRequestCommand_submit);
  *uuidreq_submitSynchronous_offset = reinterpret_cast<size_t>(&XDomainUUIDRequestCommand_submitSynchronous);
  *controlPath_sleep_offset = reinterpret_cast<size_t>(&ControlPath_sleep);
  *controlPath_wake_offset = reinterpret_cast<size_t>(&ControlPath_wake);

  // Patch bridgeProbeChild
  memset(bridgeProbeChild_patchOffset, 0x90, arrsize(IOPCIConfiguratorBridgeProbeChildLinkInterruptPatchFind));

  // Flush the cache lines containing the patched functions
  asm volatile("mfence");
  asm volatile("clflush (%0)" : : "r" (&_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController));
  asm volatile("clflush (%0)" : : "r" (bridgeProbeChild_patchOffset));
  asm volatile("clflush (%0)" : : "r" (bridgeProbeChild_patchOffset + 16));

  // Route IOPCIConfigurator functions
  IOPCIConfigurator_bridgeScanBus_orig = reinterpret_cast<void(*)(void*, IOPCIConfigEntry*, uint8_t, uint32_t)>(routeFunctionWithTrampolineInternal((void*) &_ZN17IOPCIConfigurator13bridgeScanBusEP16IOPCIConfigEntryh, (void*) &IOPCIConfigurator_bridgeScanBus));
  IOPCIConfigurator_scanProc_orig = reinterpret_cast<int32_t(*)(void*, void*, IOPCIConfigEntry*)>(routeFunctionWithTrampolineInternal((void*) &_ZN17IOPCIConfigurator8scanProcEPvP16IOPCIConfigEntry, (void*) &IOPCIConfigurator_scanProc));

  // Restore protected page write state
  if (cr0 & CR0_WP) {
    set_cr0(cr0);
  }

  // Done with no-interrupts mode
  asm volatile("sti");

  kprintf("ThunderboltEnabler: IOThunderboltFamily and IOPCIFamily code patches applied.\n");

  // Patch over the IOPCITunnelled symbol in IOPCIFamily. We want to use this symbol ourselves to mark PCI devices downstream of the TB controller
  // as removable/ejectable to the system (think eGPUs), but IOPCIFamily will see that key and reject IOPCIDevice matches unless they're IOPCITunnelCompatible.
  if (gIOPCITunnelledKey == OSSymbol::withCString("IOPCITunnelled")) {
    kprintf("ThunderboltEnabler: disabling IOPCITunnelled behavior in IOPCIFamily\n");
    gIOPCITunnelledKey = OSSymbol::withCString("TBETunnelled_disabled");
  }

  lilu.onProcLoad(&procSysUIserver, 1, nullptr, nullptr, &expressCardMenuExtraPatch, 1);

  lilu.onPatcherLoad(&TBE_onPatcherLoaded_callback, this);

  lilu.onKextLoadForce(kexts, arrsize(kexts),
    [](void* user, KernelPatcher& patcher, size_t index, mach_vm_address_t address, size_t size) {
      static_cast<TBE*>(user)->patchKext(patcher, index, address, size);
    }, this);
}

void TBE::deinit() {

}

void TBE::onPatcherLoaded(KernelPatcher& patcher) {
  kprintf("ThunderboltEnabler: TBE::onPatcherLoaded()\n");

#if LOG_PACKET_BYTES
  // Patch IOThunderboltFamily and AppleThunderboltNHI. This kext depends on both of those so they're guaranteed to be loaded first.

  origRxCommandCallback = patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(&_ZN24IOThunderboltControlPath17rxCommandCallbackEPviP27IOThunderboltReceiveCommand), reinterpret_cast<mach_vm_address_t>(&wrapRxCommandCallback), true, true);
  if (origRxCommandCallback) {
    kprintf("ThunderboltEnabler: Wrapped IOThunderboltControlPath::rxCommandCallback\n");
  } else {
    kprintf("ThunderboltEnabler: Failed to wrap IOThunderboltControlPath::rxCommandCallback: %d\n", patcher.getError());
    patcher.clearError();
  }

  origSubmitTxCommandToNHI = patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(&_ZN31AppleThunderboltNHITransmitRing18submitCommandToNHIEP28IOThunderboltTransmitCommand), reinterpret_cast<mach_vm_address_t>(&wrapSubmitTxCommandToNHI), true, true);
  if (origSubmitTxCommandToNHI) {
    kprintf("ThunderboltEnabler: Wrapped AppleThunderboltNHITransmitRing::submitCommandToNHI\n");
  } else {
    kprintf("ThunderboltEnabler: Failed to wrap AppleThunderboltNHITransmitRing::submitCommandToNHI: %d\n", patcher.getError());
    patcher.clearError();
  }
#endif
}

void TBE::patchKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
  if (index == kexts[0].loadIndex) {
    // AppleThunderboltIP. This kext ships without the OSBundleCompatible version key in its Info.plist which prevents us
    // from linking against it. Fortunately, it loads much later than the NHI and family kexts, after the kext patcher is ready.

    kprintf("TBE::patchKext: Processing kext %s\n", kexts[0].id);

    // Patch the vtable for AppleThunderboltIP
    ThunderboltIPService_start_orig = reinterpret_cast<bool(*)(void*, IOService*)>(patcher.solveSymbol(index, "__ZN25AppleThunderboltIPService5startEP9IOService", address, size));
    size_t* ipservice_vtable = reinterpret_cast<size_t*>(patcher.solveSymbol(index, "__ZTV25AppleThunderboltIPService", address, size));
    kprintf("TBE::patchKext: AppleThunderboltIPService::start() is at %p; vtable is at %p\n", reinterpret_cast<void*>(ThunderboltIPService_start_orig), ipservice_vtable);
    size_t* ipservice_start_offset = scanVtableForFunction(ipservice_vtable, reinterpret_cast<void*>(ThunderboltIPService_start_orig));
    kprintf("TBE::patchKext: ipservice_start_offset=%p\n", ipservice_start_offset);
    if (!ipservice_start_offset)
      return; // ???

    // Disable interrupts and enable writing to protected pages to patch the vtable
    asm volatile("cli");
    uintptr_t cr0 = get_cr0();
    if (cr0 & CR0_WP) {
      set_cr0(cr0 & (~CR0_WP));
    }

    *ipservice_start_offset = reinterpret_cast<size_t>(&ThunderboltIPService_start);

    // Restore CR0 and interrupt state
    if (cr0 & CR0_WP) {
      set_cr0(cr0);
    }
    asm volatile("sti");

  }
}

//// ---------------------------------------------------------------------------------------------------------------------------------- ////
#define REG_HOP_COUNT       0x39640
#define REG_INMAIL_DATA     0x39900
  
#define REG_INMAIL_CMD      0x39904
#define REG_INMAIL_CMD_MASK   GENMASK(7, 0)
#define REG_INMAIL_ERROR    BIT(30)
#define REG_INMAIL_OP_REQUEST   BIT(31)

#define REG_OUTMAIL_CMD     0x3990c
#define REG_OUTMAIL_CMD_OPMODE_SHIFT  8
#define REG_OUTMAIL_CMD_OPMODE_MASK GENMASK(11, 8)

static bool nhi_mailbox_cmd(AppleThunderboltGenericHAL* thunderboltHAL, uint32_t cmd, uint32_t data) {

  thunderboltHAL->registerWrite32(REG_INMAIL_DATA, data);
  uint32_t val = thunderboltHAL->registerRead32(REG_INMAIL_CMD);
  val &= ~(REG_INMAIL_CMD_MASK | REG_INMAIL_ERROR);
  val |= REG_INMAIL_OP_REQUEST | cmd;
  thunderboltHAL->registerWrite32(REG_INMAIL_CMD, val);

  uint64_t waittime;
  nanoseconds_to_absolutetime(500/*ms*/ * 1000000ULL, &waittime);
  uint64_t abstime_end = mach_absolute_time() + waittime;

  do {
    val = thunderboltHAL->registerRead32(REG_INMAIL_CMD);
    if (!(val & REG_INMAIL_OP_REQUEST))
      break;

    IOSleep(10);
  } while (mach_absolute_time() < abstime_end);

  if (val & REG_INMAIL_OP_REQUEST) {
    kprintf("nhi_mailbox_cmd(): request cmd=0x%x data=0x%x timed out\n", cmd, data);
    return false;
  }
  if (val & REG_INMAIL_ERROR) {
    kprintf("nhi_mailbox_cmd(): request cmd=0x%x data=0x%x failed (REG_INMAIL_ERROR)\n", cmd, data);
    return false;
  }

  return true;
}

//// ---------------------------------------------------------------------------------------------------------------------------------- ////
#define PCIE2CIO_CMD  0x30
#define PCIE2CIO_CMD_TIMEOUT      BIT(31)
#define PCIE2CIO_CMD_START        BIT(30)
#define PCIE2CIO_CMD_WRITE        BIT(21)
#define PCIE2CIO_CMD_CS_MASK      GENMASK(20, 19)
#define PCIE2CIO_CMD_CS_SHIFT     19
#define PCIE2CIO_CMD_PORT_MASK    GENMASK(18, 13)
#define PCIE2CIO_CMD_PORT_SHIFT   13

#define PCIE2CIO_WRDATA           0x34
#define PCIE2CIO_RDDATA           0x38

bool pcie2cio_read(IOPCIDevice* pciDevice, IOByteCount vendorCapOffset, uint32_t configSpace, uint32_t port, uint32_t index, uint32_t *outData) {
  uint32_t cmd = index;
  cmd |= (port << PCIE2CIO_CMD_PORT_SHIFT) & PCIE2CIO_CMD_PORT_MASK;
  cmd |= (configSpace << PCIE2CIO_CMD_CS_SHIFT) & PCIE2CIO_CMD_CS_MASK;
  cmd |= PCIE2CIO_CMD_START;
  pciDevice->extendedConfigWrite32(vendorCapOffset + PCIE2CIO_CMD, cmd);

  uint32_t cmdResult = 0;
  // wait for up to 5 seconds for the command to complete
  uint64_t waittime;
  nanoseconds_to_absolutetime(5 * 1000000000ULL, &waittime);
  uint64_t abstime_end = mach_absolute_time() + waittime;

  do {
    cmdResult = pciDevice->extendedConfigRead32(vendorCapOffset + PCIE2CIO_CMD);

    if (!(cmdResult & PCIE2CIO_CMD_START)) {
      if (cmdResult & PCIE2CIO_CMD_TIMEOUT) {
        break;
      }
      *outData = pciDevice->extendedConfigRead32(vendorCapOffset + PCIE2CIO_RDDATA);
      return true;
    }

    IOSleep(50);
  } while (mach_absolute_time() < abstime_end);

  kprintf("pcie2cio_read: command timed out. offset = 0x%llx, cmd = 0x%x, cmdResult = 0x%x\n", vendorCapOffset, cmd, cmdResult);
  return false;
}

bool pcie2cio_write(IOPCIDevice* pciDevice, IOByteCount vendorCapOffset, uint32_t configSpace, uint32_t port, uint32_t index, uint32_t data) {
  pciDevice->extendedConfigWrite32(vendorCapOffset + PCIE2CIO_WRDATA, data);

  uint32_t cmd = index;
  cmd |= (port << PCIE2CIO_CMD_PORT_SHIFT) & PCIE2CIO_CMD_PORT_MASK;
  cmd |= (configSpace << PCIE2CIO_CMD_CS_SHIFT) & PCIE2CIO_CMD_CS_MASK;
  cmd |= PCIE2CIO_CMD_WRITE | PCIE2CIO_CMD_START;

  pciDevice->extendedConfigWrite32(vendorCapOffset + PCIE2CIO_CMD, cmd);

  uint32_t cmdResult = 0;
  // wait for up to 5 seconds for the command to complete
  uint64_t waittime;
  nanoseconds_to_absolutetime(5 * 1000000000ULL, &waittime);
  uint64_t abstime_end = mach_absolute_time() + waittime;

  do {
    cmdResult = pciDevice->extendedConfigRead32(vendorCapOffset + PCIE2CIO_CMD);

    if (!(cmdResult & PCIE2CIO_CMD_START)) {
      if (cmdResult & PCIE2CIO_CMD_TIMEOUT) {
        break;
      }
      return true;
    }
  
    IOSleep(50);
  } while (mach_absolute_time() < abstime_end);

  // timed out, either on the host or by PCIE2CIO_CMD_TIMEOUT set
  kprintf("pcie2cio_write: command timed out. offset = 0x%llx, cmd = 0x%x, data = 0x%x, cmdResult = 0x%x\n", vendorCapOffset, cmd, data, cmdResult);
  return false;
}

#define PCI_EXT_CAP_ID(header)    (header & 0x0000ffff)
#define PCI_EXT_CAP_VER(header)   ((header >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(header)  ((header >> 20) & 0xffc)

size_t pci_find_ext_capability(IOPCIDevice* pci, uint32_t capID) {
  const uint32_t PCI_CFG_SPACE_SIZE = 256;
  const uint32_t PCI_CFG_SPACE_EXP_SIZE = 4096;

  uint32_t header;
  int ttl;
  int pos = PCI_CFG_SPACE_SIZE;

  /* minimum 8 bytes per capability */
  ttl = (PCI_CFG_SPACE_EXP_SIZE - PCI_CFG_SPACE_SIZE) / 8;

  //if (dev->cfg_size <= PCI_CFG_SPACE_SIZE)
  //  return 0;

  header = pci->configRead32(pos);

  /*  
   * If we have no capabilities, this is indicated by cap ID,                                                                                                       
   * cap version and next pointer all being 0.
   */
  if (header == 0)
    return 0;
 
  size_t res = 0; 

  while (ttl-- > 0) {
    //kprintf("pci[0x%p] CAP_ID %02x offset %x\n", pci, PCI_EXT_CAP_ID(header), pos);

    if (PCI_EXT_CAP_ID(header) == capID)
      res = pos;

    pos = PCI_EXT_CAP_NEXT(header);
    if (pos < PCI_CFG_SPACE_SIZE)
      break;
  
    header = pci->configRead32(pos);
  }
  
  return res;
}

#define PHY_PORT_CS1      0x37
#define PHY_PORT_CS1_LINK_DISABLE BIT(14)
#define PHY_PORT_CS1_LINK_STATE_MASK  GENMASK(29, 26)
#define PHY_PORT_CS1_LINK_STATE_SHIFT 26

enum tb_port_state {
  TB_PORT_DISABLED  = 0, /* tb_cap_phy.disable == 1 */
  TB_PORT_CONNECTING  = 1, /* retry */
  TB_PORT_UP    = 2,
  TB_PORT_UNPLUGGED = 7,
};

static int icm_reset_phy_port(IOPCIDevice* pciDevice, size_t vendorCapOffset, int phy_port) {
  uint32_t state0, state1;
  uint32_t port0, port1;
  uint32_t val0, val1;

  if (phy_port) {
    port0 = 3;
    port1 = 4;
  } else {
    port0 = 1;
    port1 = 2;
  }

  /*
   * Read link status of both null ports belonging to a single
   * physical port.
   */
  if (!pcie2cio_read(pciDevice, vendorCapOffset, TB_CFG_PORT, port0, PHY_PORT_CS1, &val0))
    return false;
  if (!pcie2cio_read(pciDevice, vendorCapOffset, TB_CFG_PORT, port1, PHY_PORT_CS1, &val1))
    return false;

  state0 = val0 & PHY_PORT_CS1_LINK_STATE_MASK;
  state0 >>= PHY_PORT_CS1_LINK_STATE_SHIFT;
  state1 = val1 & PHY_PORT_CS1_LINK_STATE_MASK;
  state1 >>= PHY_PORT_CS1_LINK_STATE_SHIFT;

  /* If they are both up we need to reset them now */
  kprintf("ThunderboltEnabler: icm_reset_phy_port(%u): state0=%u state1=%u\n", phy_port, state0, state1);
/*
  if (state0 != TB_PORT_UP || state1 != TB_PORT_UP)
    return true;
*/

  val0 |= PHY_PORT_CS1_LINK_DISABLE;
  if (!pcie2cio_write(pciDevice, vendorCapOffset, TB_CFG_PORT, port0, PHY_PORT_CS1, val0))
    return false;

  val1 |= PHY_PORT_CS1_LINK_DISABLE;
  if (!pcie2cio_write(pciDevice, vendorCapOffset, TB_CFG_PORT, port1, PHY_PORT_CS1, val1))
    return false;

  /* Wait a bit and then re-enable both ports */
  IOSleep(10);

  if (!pcie2cio_read(pciDevice, vendorCapOffset, TB_CFG_PORT, port0, PHY_PORT_CS1, &val0))
    return false;
  if (!pcie2cio_read(pciDevice, vendorCapOffset, TB_CFG_PORT, port1, PHY_PORT_CS1, &val1))
    return false;

  val0 &= ~PHY_PORT_CS1_LINK_DISABLE;
  if (!pcie2cio_write(pciDevice, vendorCapOffset, TB_CFG_PORT, port0, PHY_PORT_CS1, val0))
    return false;

  val1 &= ~PHY_PORT_CS1_LINK_DISABLE;
  if (!pcie2cio_write(pciDevice, vendorCapOffset, TB_CFG_PORT, port1, PHY_PORT_CS1, val1))
    return false;

  return true;
}


//// ---------------------------------------------------------------------------------------------------------------------------------- ////

void doICMReady(IOThunderboltController* controller) {
  IOThunderboltConfigICMCommand* icmReady = IOThunderboltConfigICMCommand::withController(controller);
  icm_pkg_driver_ready driver_ready_req;
  icmReady->setRequestData(&driver_ready_req, sizeof(icm_pkg_driver_ready));

  int res = icmReady->submitSynchronous();
  kprintf("ThunderboltEnabler: ICM ready command sent, res = %d %s\n", res, res == 0 ? "(ok)" : "(error)");
  if (res == 0) {
    icm_ar_pkg_driver_ready_response* resp = reinterpret_cast<icm_ar_pkg_driver_ready_response*>(icmReady->responseData());
    kprintf("  hdr: code=%x flags=%x packet_id=%x total_packets=%x\n", resp->hdr.code & 0xff, resp->hdr.flags & 0xff, resp->hdr.packet_id & 0xff, resp->hdr.total_packets & 0xff);
    kprintf("  romver=%x ramver=%x info=%x\n", resp->romver & 0xff, resp->ramver & 0xff, resp->info & 0xffff);
  }
  OSSafeReleaseNULL(icmReady);
}


#define REG_FW_STS  0x39944

#define REG_FW_STS_ICM_EN         BIT(0) 
#define REG_FW_STS_ICM_EN_INVERT  BIT(1)
#define REG_FW_STS_ICM_EN_CPU     BIT(2)
#define REG_FW_STS_CIO_RESET_REQ  BIT(30)
#define REG_FW_STS_NVM_AUTH_DONE  BIT(31)

#define PCI_EXT_CAP_ID_VNDR  0x0B


/*static*/ IOThunderboltConnectionManager* TBE::connectionManagerWithController(IOThunderboltController* controller) {
  kprintf("ThunderboltEnabler: connectionManagerWithController()\n");

  // IOService -> IOThunderboltNHI -> AppleThunderboltNHI -> AppleThunderboltNHIType{1,2,3}
  void* thunderboltNHI = *reinterpret_cast<void**>((reinterpret_cast<char*>(controller) + 0x88));
  // IOService -> AppleThunderboltGenericHAL -> AppleThunderboltHAL
  AppleThunderboltGenericHAL* thunderboltHAL = *reinterpret_cast<AppleThunderboltGenericHAL**>((reinterpret_cast<char*>(thunderboltNHI) + 0x90));

  // parentBridgeDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4/IOPP/PXSX@0/IOPP/pci-bridge@0 pci8086,15da
  IOPCIDevice* parentBridgeDevice = thunderboltHAL->getParentBridgeDevice();
  kprintf("ThunderboltEnabler: parentBridgeDevice @ %p\n", parentBridgeDevice);
  if (parentBridgeDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    parentBridgeDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("ThunderboltEnabler: parentBridgeDevice path is %s\n", pathBuf);
  }


  // rootBridgeDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4/IOPP/PXSX@0 pci8086,15da
  IOPCIDevice* rootBridgeDevice = thunderboltHAL->getRootBridgeDevice();
  kprintf("ThunderboltEnabler: rootBridgeDevice @ %p\n", rootBridgeDevice);
  if (rootBridgeDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    rootBridgeDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("ThunderboltEnabler: rootBridgeDevice path is %s\n", pathBuf);
  }


  // rootPortDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4 pci8086,a334
  IOPCIDevice* rootPortDevice = thunderboltHAL->getRootPortDevice();
  kprintf("ThunderboltEnabler: rootPortDevice @ %p\n", rootPortDevice);
  if (rootPortDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    rootPortDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("ThunderboltEnabler: rootPortDevice path is %s\n", pathBuf);
  }

  // Install a config message handler on the rootPortDevice so we can catch kIOMessageDeviceWillPowerOn and
  // turn on the thunderbolt force-power early enough. PCI wake happens extremely early and we can't rely
  // on the ThunderboltEnabler service attached to the IOACPIPlatformDevice waking itself in time.
  {
    OSDictionary* matchDict = IOService::serviceMatching("ThunderboltEnabler");
    IOService* tbeService = IOService::waitForMatchingService(matchDict);
    OSSafeReleaseNULL(matchDict);
    ThunderboltEnabler* tbe = OSDynamicCast(ThunderboltEnabler, tbeService);
    assert(tbe);

    IOPCIDeviceConfigHandler currentHandler;
    void* currentRef;
    rootPortDevice->setConfigHandler(&ThunderboltEnabler::pciDeviceConfigHandler, tbe, &currentHandler, &currentRef);
    tbe->setChainConfigHandler(currentHandler, currentRef);
  }

  bool configAccessEnabled = thunderboltHAL->isConfigAccessEnabled();
  kprintf("ThunderboltEnabler: configAccessEnabled = %d, pciDevice @ %p\n", configAccessEnabled, thunderboltHAL->getPCIDevice());
  if (configAccessEnabled) {
    // Config access must be disabled for MMIO register access (register{Read,Write}*)
    thunderboltHAL->enableConfigAccess(false);
  }

  int fw_status = thunderboltHAL->registerRead32(REG_FW_STS);
  kprintf("ThunderboltEnabler: REG_FW_STS = 0x%x\n", fw_status);

  if (!(fw_status & REG_FW_STS_ICM_EN)) {
    kprintf("ThunderboltEnabler: ICM is disabled.\n");

    // Don't need to replace the connection manager if the ICM is disabled.
    // Just return the original software implementation.
    IOThunderboltConnectionManager* cm = OSTypeAlloc(IOThunderboltConnectionManager);
    if (cm) {
      if (!cm->initWithController(controller)) {
        OSSafeReleaseNULL(cm);
      }
    }
    return cm;
  }


  kprintf("ThunderboltEnabler: ICM is enabled.\n");

  int hop_count =  thunderboltHAL->registerRead32(REG_HOP_COUNT) & 0x3ff;
  kprintf("ThunderboltEnabler: NHI hop count = %d\n", hop_count);


  int nhi_mode =  thunderboltHAL->registerRead32(REG_OUTMAIL_CMD);
  int icm_mode = (nhi_mode & REG_OUTMAIL_CMD_OPMODE_MASK) >> REG_OUTMAIL_CMD_OPMODE_SHIFT;
  const char* icm_mode_strings[] {
    "SAFE_MODE",
    "AUTH_MODE",
    "EP_MODE",
    "CM_MODE",
  };

  kprintf("ThunderboltEnabler: ICM mode %u (%s)\n", icm_mode, icm_mode <= 3 ? icm_mode_strings[icm_mode] : "(unknown)");

#define NHI_MAILBOX_ALLOW_ALL_DEVS  0x23
  nhi_mailbox_cmd(thunderboltHAL, NHI_MAILBOX_ALLOW_ALL_DEVS, 0);

  // Reset the PHY ports to ensure that devices that are already plugged in get discovered correctly
  size_t vendorCapOffset = pci_find_ext_capability(rootBridgeDevice, PCI_EXT_CAP_ID_VNDR);
  kprintf("ThunderboltEnabler: PCI_EXT_CAP_ID_VNDR offset = 0x%zx\n", vendorCapOffset);
  if (!icm_reset_phy_port(rootBridgeDevice, vendorCapOffset, 0))
    kprintf("ThunderboltEnabler: Failed to reset links on PHY port 0\n");
  if (!icm_reset_phy_port(rootBridgeDevice, vendorCapOffset, 1))
    kprintf("ThunderboltEnabler: Failed to reset links on PHY port 1\n");

  // Send the ICM driver ready command
  doICMReady(controller);

  // Wait for the switch config space to become accessible
  {
    IOThunderboltConfigReadCommand* swConfigRead = IOThunderboltConfigReadCommand::withController(controller);
    IOBufferMemoryDescriptor* swConfigReadMem = IOBufferMemoryDescriptor::withCapacity(0x100, 3, 0);

    swConfigRead->setResponseDataDescriptor(swConfigReadMem); 
    swConfigRead->setRouteString(0);
    swConfigRead->setPort(0);
    swConfigRead->setConfigSpace(TB_CFG_SWITCH);

    swConfigRead->setOffset(0);
    swConfigRead->setLength(1);

    int retries_remaining = 50;
    int res;
    do {
      res = swConfigRead->submitSynchronous();
      if (res == 0) {
        break;
      }
      IOSleep(50/*ms*/);
    } while (retries_remaining-->0);

    if (res == 0) {
      kprintf("ThunderboltEnabler: switch config space became accessible\n");
    } else {
      kprintf("ThunderboltEnabler: switch config space is not accessible after retry limit hit. final res = %u, error code = %u\n", res, swConfigRead->getErrorCode());
    }


    OSSafeReleaseNULL(swConfigRead);
    OSSafeReleaseNULL(swConfigReadMem);
  }

  // Install the ICM message listener
  {
    IOThunderboltICMListener* icmListener = IOThunderboltICMListener::withController(controller);
    IOReturn res = controller->getControlPath()->addListener(icmListener);
    if (res != 0) {
      kprintf("ThunderboltEnabler: ICM listener installation failed: 0x%x\n", res);
    }
    OSSafeReleaseNULL(icmListener);
  }

  // Finish creating the connection manager and return it
  {
    IOThunderboltConnectionManager* cm = OSTypeAlloc(IOThunderboltConnectionManager);
    if (cm) {
      if (!cm->initWithController(controller)) {
        OSSafeReleaseNULL(cm);
      }
    }
    return cm;
  }
}

//// ---------------------------------------------------------------------------------------------------------------------------------- ////

/*static*/ int TBE::wrapSubmitTxCommandToNHI(void* that, IOThunderboltTransmitCommand* txCommand) {
  kprintf("txCommand: sof=0x%x eof=0x%x length=0x%llx\n", txCommand->getSOF(), txCommand->getEOF(), txCommand->getLength());
  uint32_t* txMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(txCommand->getMemoryDescriptor())->getBytesNoCopy());
  size_t txWords = txCommand->getLength() / 4;
  for (size_t i = 0; i < txWords; ++i) {
    kprintf("txCommand:   %03zx  %08x\n", i * 4, txMem[i]);
  }

  int res = FunctionCast(wrapSubmitTxCommandToNHI, callbackInst->origSubmitTxCommandToNHI)(that, txCommand);
  return res;
}

/*static*/ int TBE::wrapRxCommandCallback(void* that, void* arg1, unsigned int arg2, IOThunderboltReceiveCommand* rxCommand) {
  if (rxCommand) {
    size_t rxWords = rxCommand->getReceivedLength() / 4;
    kprintf("rxCommand: sof=0x%x eof=0x%x length=0x%llx\n", rxCommand->getSOF(), rxCommand->getEOF(), rxCommand->getReceivedLength());
    uint32_t* rxMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(rxCommand->getMemoryDescriptor())->getBytesNoCopy());
    for (size_t i = 0; i < rxWords; ++i) {
      kprintf("rxCommand:   %03zx  %08x\n", i * 4, rxMem[i]);
    }
  }

  return FunctionCast(wrapRxCommandCallback, callbackInst->origRxCommandCallback)(that, arg1, arg2, rxCommand);
}

#if 0

/*static*/ bool TBE::wrapNHIStart(void* that, IOService* provider) {
  kprintf("wrapNHIStart(%p, %p)\n", that, provider);

  if (!FunctionCast(wrapNHIStart, callbackInst->origNHIStart)(that, provider)) {
    kprintf("wrapNHIStart: original IOService start() failed\n");
    return false;
  }

  // Check to see if the NHI is in ICM mode

  // actually AppleThunderboltGenericHAL*

#if 0

    // ---- This code tries (and fails) to reset the NHI out of ICM mode ----



    if (!rootBridgeDevice) {
      kprintf("NHIStart: can't access root bridge device to attempt reset.\n");
      goto jumpOut;
    }

    size_t vendorCapOffset = pci_find_ext_capability(rootBridgeDevice, PCI_EXT_CAP_ID_VNDR);
    kprintf("NHIStart: PCI_EXT_CAP_ID_VNDR offset = 0x%llx\n", vendorCapOffset);
    if (!vendorCapOffset) {
      kprintf("NHIStart: capabilities search failed, can't reset the controller\n");
      goto jumpOut;
    }


    int new_fw_status = fw_status | REG_FW_STS_CIO_RESET_REQ;
    kprintf("NHIStart: writing new first-stage REG_FW_STS 0x%x\n", new_fw_status);
    callbackInst->halRegisterWrite32(thunderboltHAL, REG_FW_STS, new_fw_status);

    new_fw_status = callbackInst->halRegisterRead32(thunderboltHAL, REG_FW_STS);
    kprintf("NHIStart: REG_FW_STS  = 0x%x\n", new_fw_status);

#if 1
    new_fw_status &= (~(REG_FW_STS_ICM_EN_INVERT | REG_FW_STS_ICM_EN_CPU | REG_FW_STS_ICM_EN));
#else
    new_fw_status &= (~(REG_FW_STS_ICM_EN_CPU | REG_FW_STS_ICM_EN));
    new_fw_status |= REG_FW_STS_ICM_EN_INVERT;
#endif

    kprintf("NHIStart: writing new second-stage REG_FW_STS 0x%x\n", new_fw_status);
    callbackInst->halRegisterWrite32(thunderboltHAL, REG_FW_STS, new_fw_status);

    // Trigger CIO reset 
    if (!pcie2cio_write(rootBridgeDevice, vendorCapOffset, TB_CFG_SWITCH, 0, 0x50, BIT(9))) {
      kprintf("NHIStart: pcie2cio_write() failed while trying to perform NHI reset on rootBridgeDevice\n");
    }

    // recheck status after reset
    fw_status = callbackInst->halRegisterRead32(thunderboltHAL, REG_FW_STS);
    kprintf("NHIStart: REG_FW_STS = 0x%x\n", fw_status);

    if (fw_status & REG_FW_STS_ICM_EN) {
      kprintf("NHIStart: reset failed, ICM still enabled\n");
    } else {
      kprintf("NHIStart: ICM disabled\n");
    }
#endif

  return true;
}
#endif

//// ---------------------------------------------------------------------------------------------------------------------------------- ////

// Patched functions for the IOThunderboltControlPath class
int ControlPath_sleep(void* that) {
  // Before sleeping, we need to tell the ICM that we're shutting down the driver and to save its state.
  IOThunderboltController* controller = *reinterpret_cast<IOThunderboltController**>((reinterpret_cast<char*>(that) + 0x10));
  void* thunderboltNHI = *reinterpret_cast<void**>((reinterpret_cast<char*>(controller) + 0x88));
  AppleThunderboltGenericHAL* thunderboltHAL = *reinterpret_cast<AppleThunderboltGenericHAL**>((reinterpret_cast<char*>(thunderboltNHI) + 0x90));

#define NHI_MAILBOX_SAVE_DEVS 0x05
  nhi_mailbox_cmd(thunderboltHAL, NHI_MAILBOX_SAVE_DEVS, 0);

  return _ZN24IOThunderboltControlPath5sleepEv(that);
}

int ControlPath_wake(void* that) {
  int res = _ZN24IOThunderboltControlPath4wakeEv(that);
  // After waking, but before returning control, we need to tell the ICM that the driver is coming online.
  IOThunderboltController* controller = *reinterpret_cast<IOThunderboltController**>((reinterpret_cast<char*>(that) + 0x10));
  doICMReady(controller);

  return res;
}

// Patched functions for the IOThunderboltConfigXDomainUUIDRequestCommand class

int XDomainUUIDRequestCommand_fillResponseFields(void* that) {
  // Get the route string and look up the UUID in the ICM XDomain registry.
  uint64_t routeString = *reinterpret_cast<uint64_t*>((reinterpret_cast<char*>(that) + 0x78));
  IOThunderboltController* controller = *reinterpret_cast<IOThunderboltController**>((reinterpret_cast<char*>(that) + 0x28));
  kprintf("XDomainUUIDRequestCommand_fillResponseFields: routeString=%llx controller=%p\n", routeString, controller);
  ICMXDomainRegistry* registry = ICMXDomainRegistry::registryForController(controller);

  ICMXDomainRegistryEntry* xde = registry->entryForRouteString(routeString);
  if (!xde) {
    kprintf("XDomainUUIDRequestCommand_fillResponseFields: no matching XDomain entry in the registry (%p) for this controller (%p)\n", registry, controller);
    return -1;
  }

  uuid_string_t uuidstr;
  uuid_unparse(xde->m_remoteUUID, uuidstr);
  kprintf("XDomainUUIDRequestCommand_fillResponseFields: found matching entry. remote UUID=%s, remote route=%llx\n", uuidstr, xde->m_remoteRoute);

  uuid_copy(reinterpret_cast<unsigned char*>(that) + 0x9f, xde->m_remoteUUID);
  uint64_t* remoteRouteString = reinterpret_cast<uint64_t*>((reinterpret_cast<char*>(that) + 0xb8));
  *remoteRouteString = xde->m_remoteRoute;

  return 0;
}

int XDomainUUIDRequestCommand_submit(void* that) {
  int res = XDomainUUIDRequestCommand_fillResponseFields(that);

  // Response fields are filled. We can now call the completion.
  _ZN44IOThunderboltConfigXDomainUUIDRequestCommand8completeEi(that, res);

  return res;
}

int XDomainUUIDRequestCommand_submitSynchronous(void* that) {
  // Just fill the response fields. Don't need to call the completion for a synchronous submit
  return XDomainUUIDRequestCommand_fillResponseFields(that);
}

bool ThunderboltIPService_start(void* that, IOService* provider) {
  // Save a pointer to the IP service on the XDomain registry for its controller so we can patch the domain UUID later
  IOThunderboltLocalNode* localNode = OSDynamicCast(IOThunderboltLocalNode, provider);
  IOThunderboltController* controller = nullptr;
  if (localNode)
    controller = localNode->getController();

  if (controller) {
    kprintf("ThunderboltEnabler: got IP service %p for controller %p\n", that, controller);
    ICMXDomainRegistry::registryForController(controller)->setIPService(reinterpret_cast<AppleThunderboltIPService*>(that));
  } else {
    kprintf("ThunderboltEnabler: ThunderboltIPService_start(): can't get the controller for local node provider %p\n", provider);
  }

  // Call through to the original function
  return ThunderboltIPService_start_orig(that, provider);
}

bool isThunderboltBridge(uint32_t vendorProduct) {
  uint16_t vendor = vendorProduct & 0xffff;
  uint16_t product = (vendorProduct >> 16) & 0xffff;

  if (vendor != 0x8086)
    return false;

  switch (product) {
    case PCI_DEVICE_ID_INTEL_WIN_RIDGE_2C_BRIDGE:
    case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_LP_BRIDGE:
    case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_C_4C_BRIDGE:
    case PCI_DEVICE_ID_INTEL_ALPINE_RIDGE_C_2C_BRIDGE:
    case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_2C_BRIDGE:
    case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_4C_BRIDGE:
    case PCI_DEVICE_ID_INTEL_TITAN_RIDGE_DD_BRIDGE:
      return true;

    default:
      return false;
  }
}

void IOPCIConfigurator_bridgeScanBus(void* that, IOPCIConfigEntry* device, uint8_t busNum, uint32_t resetMask) {
#if 0
  kprintf("ThunderboltEnabler: IOPCIConfigurator::bridgeScanBus: i[%x]%u:%u:%u(0x%x:0x%x) dtEntry=%p acpiDevice=%p\n",
    device->id, device->space.s.busNum, device->space.s.deviceNum, device->space.s.functionNum,
    (device->vendorProduct & 0xffff), (device->vendorProduct >> 16),
    device->dtEntry, device->acpiDevice);
#endif

  if (isThunderboltBridge(device->vendorProduct)) {
    if ((device->parent == nullptr) || !isThunderboltBridge(device->parent->vendorProduct)) {
      kprintf("ThunderboltEnabler: device is a Thunderbolt upstream bridge\n");

    } else {
      kprintf("ThunderboltEnabler: device is Thunderbolt downstream bridge %u\n", device->space.s.deviceNum);

      if (device->space.s.deviceNum == 1) { // DSB1
        if (device->dtEntry && (!device->dtEntry->getProperty("TBEPCIReady"))) {
          kprintf("ThunderboltEnabler: DSB1 does not have TBEPCIReady set, deferring bridge scan.\n");
          return;
        }
      }
    }
  }


  IOPCIConfigurator_bridgeScanBus_orig(that, device, busNum, resetMask);
}

int32_t IOPCIConfigurator_scanProc(void* that, void* ref, IOPCIConfigEntry* bridge) {
#if 0
  kprintf("ThunderboltEnabler: scanProc(%p)\n", bridge);
#endif

  int32_t res = IOPCIConfigurator_scanProc_orig(that, ref, bridge);

  if (isThunderboltBridge(bridge->vendorProduct)) {
    for (IOPCIConfigEntry* child = bridge->child; child; child = child->peer) {
      if (child->space.s.deviceNum == 1 && isThunderboltBridge(child->vendorProduct)) {
        // DSB1 of a Thunderbolt bridge complex. Mark it as a hotplug root and turn on range splay.
        // This would've been done by bridgeConnectDeviceTree if the device had a DT entry and it was
        // marked removable in ACPI (see IOPCIIsHotplugPort), but we can just set that up here to avoid
        // relying on an ACPI/DT setup.
        kprintf("ThunderboltEnabler: IOPCIConfigurator::scanProc(%p): child %p (i[%x]%u:%u:%u(0x%x:0x%x) identified as DSB1, marking kPCIHotPlugRoot and enabling splay\n",
          bridge, child, child->id, child->space.s.busNum, child->space.s.deviceNum, child->space.s.functionNum, (child->vendorProduct & 0xffff), (child->vendorProduct >> 16));

        child->supportsHotPlug = kPCIHotPlugRoot;

        for (int i = kIOPCIRangeBridgeMemory; i < kIOPCIRangeCount; i++) {
          if (!child->ranges[i])
            continue;

          child->ranges[i]->flags |= kIOPCIRangeFlagSplay;
        }
      }
    }
  }
  return res;
}

