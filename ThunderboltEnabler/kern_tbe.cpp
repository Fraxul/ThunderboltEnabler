#include "kern_tbe.hpp"

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
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
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <i386/proc_reg.h>

#include "tb_constants.h"

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

  // AppleThunderboltNHI.kext
  int _ZN31AppleThunderboltNHITransmitRing18submitCommandToNHIEP28IOThunderboltTransmitCommand(void*, IOThunderboltTransmitCommand*);
}

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

static void TBE_onPatcherLoaded_callback(void* that, KernelPatcher& patcher) { reinterpret_cast<TBE*>(that)->onPatcherLoaded(patcher); }

void TBE::init() {
  callbackInst = this;
  
  kprintf("ThunderboltEnabler: TBE::init()\n");
  ICMXDomainRegistry::staticInit();


  // We need to make a very early patch to IOThunderboltConnectionManager::withController() to ensure that we can hook the TB controller
  // initialization at the correct time. We need to redirect this function before Lilu's patcher loads, so we can't use it.
  // We don't need to preserve the original (which would require making a trampoline), though, so it's a little easier: we just clobber
  // the front of the function with a relative jump instruction and then reimplement it in the end of our replacement function.

  // Compute the function offset
  ssize_t offset = reinterpret_cast<size_t>(&connectionManagerWithController) - reinterpret_cast<size_t>(&_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController);
  offset -= 5; // adjust the offset for the 5 byte length of our JMP instruction
  int32_t offset32 = (int32_t) offset;
  kprintf("ThunderboltEnabler: IOThunderboltConnectionManager::withController @ 0x%p, TBE::connectionManagerWithController @ 0x%p\n",
    &_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController, &connectionManagerWithController);

  kprintf("ThunderboltEnabler: offset64 = 0x%zx, offset32 = 0x%x\n", offset, offset32);
  if (offset != offset32) {
    kprintf("ThunderboltEnabler: ERROR: offset doesn't fit into int32? can't assemble patch.\n");
    return;
  }

  kprintf("ThunderboltEnabler: Scanning vtable at %p\n", &_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand);
  // Compute offsets to patch in the IOThunderboltConfigXDomainUUIDRequestCommand vtable
  size_t* uuidreq_submit_offset = scanVtableForFunction(&_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand, reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand6submitEv));
  kprintf("ThunderboltEnabler: IOThunderboltConfigXDomainUUIDRequestCommand vtable contains IOThunderboltConfigCommand::submit (%p) at %p\n", reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand6submitEv), uuidreq_submit_offset);
  size_t* uuidreq_submitSynchronous_offset = scanVtableForFunction(&_ZTV44IOThunderboltConfigXDomainUUIDRequestCommand, reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand17submitSynchronousEv));
  kprintf("ThunderboltEnabler: IOThunderboltConfigXDomainUUIDRequestCommand vtable contains IOThunderboltConfigCommand::submitSynchronous (%p) at %p\n", reinterpret_cast<void*>(&_ZN26IOThunderboltConfigCommand17submitSynchronousEv), uuidreq_submitSynchronous_offset);

  if (!(uuidreq_submit_offset && uuidreq_submitSynchronous_offset)) {
    kprintf("ThunderboltEnabler: ERROR: Unable to find some vtable offsets to patch, bailing out.\n");
    return;
  }

  // Save original AppleThunderboltIPService::start() function pointer
  

  // Disable interrupts while we mess around with CR0
  asm volatile("cli");

  // Enable writing to protected pages
  uintptr_t cr0 = get_cr0();
  if (cr0 & CR0_WP) {
    set_cr0(cr0 & (~CR0_WP));
  }

  // Assemble a relative JMP
  uint8_t* p = reinterpret_cast<uint8_t*>(&_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController);
  *(p++) = 0xe9; // JMP rel32off
  *(p++) = offset32 & 0xff;
  *(p++) = (offset32 >> 8) & 0xff;
  *(p++) = (offset32 >> 16) & 0xff;
  *(p++) = (offset32 >> 24) & 0xff;

  // Fill the rest of the line with NOPs
  for (size_t i = 0; i < (16 - 5); ++i)
    *(p++) = 0x90;

  // Flush the cache line containing the patched function
  asm volatile("mfence");
  asm volatile("clflush (%0)" : : "r" (&_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController));

  // Patch vtables
  *uuidreq_submit_offset = reinterpret_cast<size_t>(&XDomainUUIDRequestCommand_submit);
  *uuidreq_submitSynchronous_offset = reinterpret_cast<size_t>(&XDomainUUIDRequestCommand_submitSynchronous);

  // Restore protected page write state
  if (cr0 & CR0_WP) {
    set_cr0(cr0);
  }

  // Done with no-interrupts mode
  asm volatile("sti");

  kprintf("ThunderboltEnabler: done applying patch to IOThunderboltConnectionManager::withController()\n");

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
bool pcie2cio_write(IOPCIDevice* pciDevice, IOByteCount vendorCapOffset, uint32_t configSpace, uint32_t port, uint32_t index, uint32_t data) {
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
    kprintf("pci[0x%p] CAP_ID %02x offset %x\n", pci, PCI_EXT_CAP_ID(header), pos);

    if (PCI_EXT_CAP_ID(header) == capID)
      res = pos;

    pos = PCI_EXT_CAP_NEXT(header);
    if (pos < PCI_CFG_SPACE_SIZE)
      break;
  
    header = pci->configRead32(pos);
  }
  
  return res;
}

//// ---------------------------------------------------------------------------------------------------------------------------------- ////

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

  kprintf("ThunderboltEnabler: ICM mode %u (%s)\n", icm_mode, icm_mode_strings[icm_mode]);

  if (icm_mode == 0) {
    kprintf("ThunderboltEnabler: ICM is in safe mode.\n");
  } else if (icm_mode == 3) { // CM_MODE
#define NHI_MAILBOX_ALLOW_ALL_DEVS  0x23
    nhi_mailbox_cmd(thunderboltHAL, NHI_MAILBOX_ALLOW_ALL_DEVS, 0);
  } else {
    kprintf("ThunderboltEnabler: ICM is in unknown mode.\n");
  }

  // TODO: may need icm_reset_phy_port on both physical ports.

  // Send the ICM driver ready command
  {
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
  kprintf("wrapSubmitTxCommandToNHI: sof=0x%x eof=0x%x offset=0x%llx size=0x%llx\n", txCommand->getSOF(), txCommand->getEOF(), txCommand->getOffset(), txCommand->getLength());
  uint32_t* txMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(txCommand->getMemoryDescriptor())->getBytesNoCopy());
  size_t txWords = txCommand->getLength() / 4;
  for (size_t i = 0; i < txWords; ++i) {
    kprintf("wrapSubmitTxCommandToNHI:   %03zx  %08x\n", i * 4, txMem[i]);
  }

  int res = FunctionCast(wrapSubmitTxCommandToNHI, callbackInst->origSubmitTxCommandToNHI)(that, txCommand);
  kprintf("wrapSubmitTxCommandToNHI: res=0x%x\n", res);
  return res;
}

/*static*/ int TBE::wrapRxCommandCallback(void* that, void* arg1, unsigned int arg2, IOThunderboltReceiveCommand* rxCommand) {
  kprintf("wrapRxCommandCallback: rxCommand=%p\n", rxCommand);
  if (rxCommand) {
    size_t rxWords = rxCommand->getReceivedLength() / 4;
    kprintf("rxCommand sof=0x%x eof=0x%x\n", rxCommand->getSOF(), rxCommand->getEOF());
    uint32_t* rxMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(rxCommand->getMemoryDescriptor())->getBytesNoCopy());
    for (size_t i = 0; i < rxWords; ++i) {
      kprintf("   %03zx  %08x\n", i * 4, rxMem[i]);
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
