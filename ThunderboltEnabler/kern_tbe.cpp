#include "kern_tbe.hpp"

#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <Headers/kern_iokit.hpp>
#include "IOThunderboltConfigICMCommand.h"
#include "IOThunderboltConfigReadCommand.h"
#include "IOThunderboltConnectionManager.h"
#include "IOThunderboltController.h"
#include "IOThunderboltControlPath.h"
#include "IOThunderboltICMListener.h"
#include "IOThunderboltReceiveCommand.h"
#include "IOThunderboltTransmitCommand.h"
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "tb_constants.h"

#define LOG_REGISTER_RW 0
#define LOG_PACKET_BYTES 0

TBE* TBE::callbackInst;

static const char* pathAppleThunderboltNHI[] {
  "/System/Library/Extensions/AppleThunderboltNHI.kext/Contents/MacOS/AppleThunderboltNHI",
  "/Users/dweatherford/thunderbolt-kexts/AppleThunderboltNHI.kext/Contents/MacOS/AppleThunderboltNHI"
};

/*
static const char* pathAppleThunderboltPCIDownAdapter[] {
  "/System/Library/Extensions/AppleThunderboltPCIAdapters.kext/Contents/PlugIns/AppleThunderboltPCIDownAdapter.kext/Contents/MacOS/AppleThunderboltPCIDownAdapter",
};
  // {"com.apple.driver.AppleThunderboltPCIDownAdapter", pathAppleThunderboltPCIDownAdapter, arrsize(pathAppleThunderboltPCIDownAdapter), {true}, {}, KernelPatcher::KextInfo::Unloaded},
*/

static KernelPatcher::KextInfo kexts[] {
  {"com.apple.driver.AppleThunderboltNHI", pathAppleThunderboltNHI, arrsize(pathAppleThunderboltNHI), {true}, {}, KernelPatcher::KextInfo::Unloaded},
};

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


// Linker stub for function routing
extern "C" {
  void* _ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController(void*);
  int _ZN24IOThunderboltControlPath17rxCommandCallbackEPviP27IOThunderboltReceiveCommand(void*, void*, unsigned int, void*);
}

void TBE::onPatcherLoaded(KernelPatcher& patcher) {
  kprintf("ThunderboltEnabler: TBE::onPatcherLoaded()\n");

  // Patch IOThunderboltFamily. This kext depends on IOThunderboltFamily so it is guaranteed to be loaded first.
  origConnectionManagerWithController = patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(&_ZN30IOThunderboltConnectionManager14withControllerEP23IOThunderboltController), reinterpret_cast<mach_vm_address_t>(&wrapConnectionManagerWithController), true, true);
  if (origConnectionManagerWithController) {
    kprintf("ThunderboltEnabler: Wrapped IOThunderboltConnectionManager::withController\n");
  } else {
    kprintf("ThunderboltEnabler: Failed to wrap IOThunderboltConnectionManager::withController: %d\n", patcher.getError());
    patcher.clearError();
  }

#if LOG_PACKET_BYTES
  origRxCommandCallback = patcher.routeFunction(reinterpret_cast<mach_vm_address_t>(&_ZN24IOThunderboltControlPath17rxCommandCallbackEPviP27IOThunderboltReceiveCommand), reinterpret_cast<mach_vm_address_t>(&wrapRxCommandCallback), true, true);
  if (origRxCommandCallback) {
    kprintf("ThunderboltEnabler: Wrapped IOThunderboltControlPath::rxCommandCallback\n");
  } else {
    kprintf("ThunderboltEnabler: Failed to wrap IOThunderboltControlPath::rxCommandCallback: %d\n", patcher.getError());
    patcher.clearError();
  }
#endif

}

static void TBE_onPatcherLoaded_callback(void* that, KernelPatcher& patcher) { reinterpret_cast<TBE*>(that)->onPatcherLoaded(patcher); }

void TBE::init() {
  callbackInst = this;
  
  kprintf("ThunderboltEnabler: TBE::init()\n");

  lilu.onProcLoad(&procSysUIserver, 1, nullptr, nullptr, &expressCardMenuExtraPatch, 1);

  lilu.onKextLoadForce(kexts, arrsize(kexts),
    [](void* user, KernelPatcher& patcher, size_t index, mach_vm_address_t address, size_t size) {
      static_cast<TBE*>(user)->patchKext(patcher, index, address, size);
    }, this);


  lilu.onPatcherLoad(&TBE_onPatcherLoaded_callback, this);
}

void TBE::deinit() {

}

void TBE::patchKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {
  if (index == kexts[0].loadIndex) {
    // AppleThunderboltNHI
    kprintf("TBE::patchKext: Processing kext %s\n", kexts[0].id);

#if LOG_REGISTER_RW
    {
      KernelPatcher::RouteRequest requests[] {
        {"__ZN26AppleThunderboltGenericHAL14registerRead32Ej", wrapHalRegisterRead32, halRegisterRead32_addr},
        {"__ZN26AppleThunderboltGenericHAL15registerWrite32Ejj", wrapHalRegisterWrite32, halRegisterWrite32_addr},
      };
      kprintf("ThunderboltEnabler: TBE::patchKext(): LOG_REGISTER_RW is enabled, applying %zu patches\n", arrsize(requests));;
      patcher.routeMultiple(index, requests, address, size);
      patcher.clearError();
    }
#endif

#if LOG_PACKET_BYTES
    {
      KernelPatcher::RouteRequest requests[] {
        {"__ZN31AppleThunderboltNHITransmitRing18submitCommandToNHIEP28IOThunderboltTransmitCommand", wrapSubmitTxCommandToNHI, origSubmitTxCommandToNHI},
      };
      kprintf("ThunderboltEnabler: TBE::patchKext(): LOG_PACKET_BYTES is enabled, applying %zu patches\n", arrsize(requests));;
      patcher.routeMultiple(index, requests, address, size);
      patcher.clearError();
    }
#endif

    // Solve necessary symbols in AppleThunderboltNHI.kext
    // TODO: just convert the vtables to headers and call these directly

#if !LOG_REGISTER_RW // need to solve these to call them if we didn't wrap them before
    halRegisterRead32_addr = patcher.solveSymbol(index, "__ZN26AppleThunderboltGenericHAL14registerRead32Ej", address, size);
    halRegisterWrite32_addr = patcher.solveSymbol(index, "__ZN26AppleThunderboltGenericHAL15registerWrite32Ejj", address, size);
#endif
    halGetParentBridgeDevice_addr = patcher.solveSymbol(index, "__ZN26AppleThunderboltGenericHAL21getParentBridgeDeviceEv", address, size);
    halGetRootBridgeDevice_addr = patcher.solveSymbol(index, "__ZN26AppleThunderboltGenericHAL19getRootBridgeDeviceEv", address, size);
    halGetRootPortDevice_addr = patcher.solveSymbol(index, "__ZN26AppleThunderboltGenericHAL17getRootPortDeviceEv", address, size);
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

bool TBE::nhi_mailbox_cmd(void* thunderboltHAL, uint32_t cmd, uint32_t data) {

  halRegisterWrite32(thunderboltHAL, REG_INMAIL_DATA, data);
  uint32_t val = halRegisterRead32(thunderboltHAL, REG_INMAIL_CMD);
  val &= ~(REG_INMAIL_CMD_MASK | REG_INMAIL_ERROR);
  val |= REG_INMAIL_OP_REQUEST | cmd;
  halRegisterWrite32(thunderboltHAL, REG_INMAIL_CMD, val);

  uint64_t waittime;
  nanoseconds_to_absolutetime(500/*ms*/ * 1000000ULL, &waittime);
  uint64_t abstime_end = mach_absolute_time() + waittime;

  do {
    val = halRegisterRead32(thunderboltHAL, REG_INMAIL_CMD);
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

struct icm_pkg_driver_ready {
  icm_pkg_driver_ready() {
    hdr.code = ICM_DRIVER_READY;
  }
  icm_pkg_header hdr;
};

struct icm_ar_pkg_driver_ready_response {
  icm_pkg_header hdr;
  uint8_t romver;
  uint8_t ramver;
  uint16_t info;
};

//// ---------------------------------------------------------------------------------------------------------------------------------- ////

#define REG_FW_STS  0x39944

#define REG_FW_STS_ICM_EN         BIT(0) 
#define REG_FW_STS_ICM_EN_INVERT  BIT(1)
#define REG_FW_STS_ICM_EN_CPU     BIT(2)
#define REG_FW_STS_CIO_RESET_REQ  BIT(30)
#define REG_FW_STS_NVM_AUTH_DONE  BIT(31)

#define PCI_EXT_CAP_ID_VNDR  0x0B


/*static*/ IOThunderboltConnectionManager* TBE::wrapConnectionManagerWithController(IOThunderboltController* controller) {
  kprintf("ThunderboltEnabler: wrapConnectionManagerWithController()\n");

  // IOService -> IOThunderboltNHI -> AppleThunderboltNHI -> AppleThunderboltNHIType{1,2,3}
  void* thunderboltNHI = *reinterpret_cast<void**>((reinterpret_cast<char*>(controller) + 0x88));
  // IOService -> AppleThunderboltGenericHAL -> AppleThunderboltHAL
  void* thunderboltHAL = *reinterpret_cast<void**>((reinterpret_cast<char*>(thunderboltNHI) + 0x90));

  // parentBridgeDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4/IOPP/PXSX@0/IOPP/pci-bridge@0 pci8086,15da
  IOPCIDevice* parentBridgeDevice = callbackInst->halGetParentBridgeDevice(thunderboltHAL);
  kprintf("NHIStart: parentBridgeDevice @ %p\n", parentBridgeDevice);
  if (parentBridgeDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    parentBridgeDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("NHIStart: parentBridgeDevice path is %s\n", pathBuf);
  }


  // rootBridgeDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4/IOPP/PXSX@0 pci8086,15da
  IOPCIDevice* rootBridgeDevice = callbackInst->halGetRootBridgeDevice(thunderboltHAL);
  kprintf("NHIStart: rootBridgeDevice @ %p\n", rootBridgeDevice);
  if (rootBridgeDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    rootBridgeDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("NHIStart: rootBridgeDevice path is %s\n", pathBuf);
  }


  // rootPortDevice: IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/RP13@1D,4 pci8086,a334
  IOPCIDevice* rootPortDevice = callbackInst->halGetRootPortDevice(thunderboltHAL);
  kprintf("NHIStart: rootPortDevice @ %p\n", rootPortDevice);
  if (rootPortDevice) {
    char pathBuf[1024];
    int pathLen = 1023;
    rootPortDevice->getPath(pathBuf, &pathLen, gIOServicePlane);
    pathBuf[pathLen] = '\0';
    kprintf("NHIStart: rootPortDevice path is %s\n", pathBuf);
  }


  int fw_status = callbackInst->halRegisterRead32(thunderboltHAL, REG_FW_STS);
  kprintf("ThunderboltEnabler: REG_FW_STS = 0x%x\n", fw_status);

  if (!(fw_status & REG_FW_STS_ICM_EN)) {
    kprintf("ThunderboltEnabler: ICM is disabled.\n");

    // Don't need to replace the connection manager if the ICM is disabled.
    // Just return the original software implementation.
    return FunctionCast(wrapConnectionManagerWithController, callbackInst->origConnectionManagerWithController)(controller);
  }


  kprintf("ThunderboltEnabler: ICM is enabled.\n");

  int hop_count =  callbackInst->halRegisterRead32(thunderboltHAL, REG_HOP_COUNT) & 0x3ff;
  kprintf("ThunderboltEnabler: NHI hop count = %d\n", hop_count);


  int nhi_mode =  callbackInst->halRegisterRead32(thunderboltHAL, REG_OUTMAIL_CMD);
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
    callbackInst->nhi_mailbox_cmd(thunderboltHAL, NHI_MAILBOX_ALLOW_ALL_DEVS, 0);
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

  // Temporary: just return the original connection manager implementation
  return FunctionCast(wrapConnectionManagerWithController, callbackInst->origConnectionManagerWithController)(controller);

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

/*static*/ uint32_t TBE::wrapHalRegisterRead32(void* that, uint32_t address) {
  void* pciDevice = *reinterpret_cast<void**>((reinterpret_cast<char*>(that) + 0x88));
  uint8_t configAccessIsEnabled = *reinterpret_cast<uint8_t*>((reinterpret_cast<char*>(that) + 0x120));

  if (!pciDevice) {
    kprintf("wrapHalRegisterRead32(%p, 0x%x): pciDevice is NULL\n", that, address);
  }
  if (configAccessIsEnabled) {
    kprintf("wrapHalRegisterRead32(%p, 0x%x): configAccess state is bad\n", that, address);
  }

  uint32_t res = callbackInst->halRegisterRead32(that, address);
  kprintf("wrapHalRegisterRead32(%p): 0x%x -> 0x%x\n", that, address, res);
  return res;
}

/*static*/ void TBE::wrapHalRegisterWrite32(void* that, uint32_t address, uint32_t value) {
  void* pciDevice = *reinterpret_cast<void**>((reinterpret_cast<char*>(that) + 0x88));
  uint8_t configAccessIsEnabled = *reinterpret_cast<uint8_t*>((reinterpret_cast<char*>(that) + 0x120));

  if (!pciDevice) {
    kprintf("wrapHalRegisterWrite32(%p, 0x%x, 0x%x): pciDevice is NULL\n", that, address, value);
  }
  if (configAccessIsEnabled) {
    kprintf("wrapHalRegisterWrite32(%p, 0x%x, 0x%x): bad configAccess state\n", that, address, value);
  }

  callbackInst->halRegisterWrite32(that, address, value);
}

