#include "IOThunderboltICMListener.h"
#include "AppleThunderboltGenericHAL.h"
#include "ICMXDomainRegistry.h"
#include "IOThunderboltConnectionManager.h"
#include "IOThunderboltController.h"
#include "IOThunderboltLocalNode.h"
#include "IOThunderboltPort.h"
#include "IOThunderboltReceiveCommand.h"
#include "IOThunderboltSwitch.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOTimerEventSource.h>

#include "tb_constants.h"

// Defined in IOThunderboltFamily
extern "C" {
  uint32_t IOThunderboltCRC32(void*, uint32_t);
};

OSDefineMetaClassAndStructors(IOThunderboltICMListener, IOThunderboltControlPathListener);

/*static*/  IOThunderboltICMListener* IOThunderboltICMListener::withController(IOThunderboltController* controller) {
  IOThunderboltICMListener* cmd = OSTypeAlloc(IOThunderboltICMListener);
  if (cmd) {
    if (!cmd->initWithController(controller)) {
      OSSafeReleaseNULL(cmd);
    }
  }
  return cmd;
}

bool IOThunderboltICMListener::initWithController(IOThunderboltController* controller_) {
  if (!IOThunderboltControlPathListener::initWithController(controller_)) {
    return false;
  }

  m_controller = controller_;

  m_rescanDelayTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOThunderboltICMListener::delayedRescanTimerFired));
  m_controller->getWorkLoop()->addEventSource(m_rescanDelayTimer);

  OSDictionary* matchDict = IOService::serviceMatching("IOThunderboltPort");
  m_portPublishedNotification = IOService::addMatchingNotification(gIOPublishNotification, matchDict, OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOThunderboltICMListener::handleThunderboltPortPublishedNotification), this, NULL);
  m_portTerminatedNotification = IOService::addMatchingNotification(gIOTerminatedNotification, matchDict, OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOThunderboltICMListener::handleThunderboltPortTerminatedNotification), this, NULL);
  OSSafeReleaseNULL(matchDict);

  matchDict = IOService::serviceMatching("IOPCIDevice");
  m_pciDevicePublishedNotification = IOService::addMatchingNotification(gIOPublishNotification, matchDict, OSMemberFunctionCast(IOServiceMatchingNotificationHandler, this, &IOThunderboltICMListener::handlePCIDevicePublishedNotification), this, NULL);
  OSSafeReleaseNULL(matchDict);

  // IOService -> IOThunderboltNHI -> AppleThunderboltNHI -> AppleThunderboltNHIType{1,2,3}
  void* thunderboltNHI = *reinterpret_cast<void**>((reinterpret_cast<char*>(controller_) + 0x88));
  // IOService -> AppleThunderboltGenericHAL -> AppleThunderboltHAL
  AppleThunderboltGenericHAL* thunderboltHAL = *reinterpret_cast<AppleThunderboltGenericHAL**>((reinterpret_cast<char*>(thunderboltNHI) + 0x90));

  IOPCIDevice* rootBridgeDevice = thunderboltHAL->getRootBridgeDevice(); // PXSX upstream of DSB{0,1,2}

  m_dsb1 = NULL;
  {
    OSIterator* dsbIterator = rootBridgeDevice->getChildIterator(gIODTPlane);
    for (OSObject* itObj = dsbIterator->getNextObject(); itObj; itObj = dsbIterator->getNextObject()) {
      IOPCIDevice* itPCI = OSDynamicCast(IOPCIDevice, itObj);
      if (!itPCI)
        continue; // ???
      if (itPCI->getDeviceNumber() == 1) {
        m_dsb1 = itPCI;
        break;
      }
    }
    OSSafeReleaseNULL(dsbIterator);
  }
  if (!m_dsb1) {
    kprintf("ThunderboltEnabler: Couldn't find DSB1 (PCI device with device-number 1) under root bridge device\n");
  }


  return true;
}

void IOThunderboltICMListener::free() {
  m_controller->getWorkLoop()->removeEventSource(m_rescanDelayTimer);
  OSSafeReleaseNULL(m_rescanDelayTimer);

  OSSafeReleaseNULL(m_portPublishedNotification);
  OSSafeReleaseNULL(m_portTerminatedNotification);
  OSSafeReleaseNULL(m_pciDevicePublishedNotification);

  ICMXDomainRegistry::willRetireController(m_controller);

  IOThunderboltControlPathListener::free();
}

bool IOThunderboltICMListener::handleThunderboltPortPublishedNotification(void*, IOService* service, IONotifier*) {
  char pathBuf[1024];
  int pathLen = 1023;
  service->getPath(pathBuf, &pathLen, gIOServicePlane);

  kprintf("ThunderboltEnabler: Thunderbolt port %s was published, queueing rescan of PCI devices under DSB1\n", pathBuf);
  // Setting a short timeout will effectively coalesce the publish notifications
  m_rescanDelayTimer->setTimeoutMS(100);
  return true;
}

bool IOThunderboltICMListener::handleThunderboltPortTerminatedNotification(void*, IOService* service, IONotifier*) {
  char pathBuf[1024];
  int pathLen = 1023;
  service->getPath(pathBuf, &pathLen, gIOServicePlane);

  kprintf("ThunderboltEnabler: Thunderbolt port %s was terminated, queueing rescan of PCI devices under DSB1\n", pathBuf);
  m_rescanDelayTimer->setTimeoutMS(100);
  return true;
}

bool IOThunderboltICMListener::handlePCIDevicePublishedNotification(void* refCon, IOService* service, IONotifier*) {
  char pathBuf[1024];
  int pathLen = 1023;
  service->getPath(pathBuf, &pathLen, gIOServicePlane);

  for (IORegistryEntry* ancestor = service->getParentEntry(gIOServicePlane); ancestor != IORegistryEntry::getRegistryRoot(); ancestor = ancestor->getParentEntry(gIOServicePlane)) {
    IOPCIDevice* pci = OSDynamicCast(IOPCIDevice, ancestor);
    if (!pci)
      continue;
    if (pci == m_dsb1) {
      kprintf("ThunderboltEnabler: Published PCI device %s is under DSB1, marking as IOPCITunnelled\n", pathBuf);
      service->setProperty("IOPCITunnelled", kOSBooleanTrue);
      return true;
    }
  }


  return true;
}

void IOThunderboltICMListener::delayedRescanTimerFired(OSObject* owner, IOTimerEventSource*) {
  rescanDSB1();
}

void IOThunderboltICMListener::rescanDSB1() {
  if (!m_dsb1) {
    kprintf("ThunderboltEnabler: Couldn't queue rescan as we couldn't find DSB1 during initialization\n");
    return;
  }

  if (!m_dsb1->getProperty("TBEPCIReady")) {
    kprintf("ThunderboltEnabler: First requested rescan of DSB1, marking TBEPCIReady\n");
    m_dsb1->setProperty("TBEPCIReady", kOSBooleanTrue);
  }

  IORegistryIterator* regIt = IORegistryIterator::iterateOver(m_dsb1, gIOServicePlane, kIORegistryIterateRecursively);
  for (IORegistryEntry* itObj = regIt->getNextObject(); itObj; itObj = regIt->getNextObject()) {
    IOPCIDevice* itPCI = OSDynamicCast(IOPCIDevice, itObj);
    if (!itPCI)
      continue;
    itPCI->kernelRequestProbe(kIOPCIProbeOptionNeedsScan);
  }

  m_dsb1->kernelRequestProbe(kIOPCIProbeOptionNeedsScan | kIOPCIProbeOptionDone);
}

void IOThunderboltICMListener::processResponse(IOThunderboltReceiveCommand* rxCommand) {
  kprintf("IOThunderboltICMListener::processResponse: rxCommand=%p\n", rxCommand);
  if (!rxCommand)
    return;

  if (rxCommand->getEOF() != TB_CFG_PKG_ICM_EVENT)
    return; // not relevant

  uint32_t* rxMemRaw = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(rxCommand->getMemoryDescriptor())->getBytesNoCopy());
  size_t rxWords = rxCommand->getReceivedLength() / 4;

  uint32_t crc_wire = OSSwapBigToHostInt32(rxMemRaw[rxWords - 1]);
  uint32_t crc_computed = IOThunderboltCRC32(rxMemRaw, (uint32_t) ((rxWords - 1) * 4));
  if (crc_wire != crc_computed) {
    kprintf("IOThunderboltICMListener::processResponse(): bad CRC: wire is %08x, computed is %08x\n", crc_wire, crc_computed);
    // TODO CRC computation is not working right, so we don't bail here yet
  }

  // Byteswap into a local buffer. Maximum packet size is 256 bytes / 64 dwords.
  uint32_t rxMem[64];
  bzero(rxMem, 256);
  for (size_t i = 0; i < rxWords; ++i) {
    rxMem[i] = OSSwapBigToHostInt32(rxMemRaw[i]);
  }

  icm_pkg_header* header = reinterpret_cast<icm_pkg_header*>(rxMem);
  kprintf("icm_pkg_header code=%x flags=%x packet_id=%x total_packets=%x\n", header->code, header->flags, header->packet_id, header->total_packets);

  switch (header->code) {

    case ICM_EVENT_DEVICE_CONNECTED:
      handleDeviceConnected(reinterpret_cast<icm_fr_event_device_connected*>(rxMem));
      break;

    case ICM_EVENT_DEVICE_DISCONNECTED:
      handleDeviceDisconnected(reinterpret_cast<icm_fr_event_device_disconnected*>(rxMem));
      break;

    case ICM_EVENT_XDOMAIN_CONNECTED:
      handleXDomainConnected(reinterpret_cast<icm_fr_event_xdomain_connected*>(rxMem));
      break;

    case ICM_EVENT_XDOMAIN_DISCONNECTED:
      handleXDomainDisconnected(reinterpret_cast<icm_fr_event_xdomain_disconnected*>(rxMem));
      break;

    default:
      kprintf("IOThunderboltICMListener: Unhandled ICM event type %x\n", header->code);
      break;
  };
}

uint64_t combine_route(uint32_t route_hi, uint32_t route_lo) {
  return ((static_cast<uint64_t>(route_hi) << 32) | static_cast<uint64_t>(route_lo));
}


void IOThunderboltICMListener::handleDeviceConnected(icm_fr_event_device_connected* evt) {
  uuid_string_t uuidstr;
  uuid_unparse(evt->endpoint_uuid, uuidstr);

  char* vendor = evt->endpoint_name;
  // Vendor and device names are NULL-separated in the result array
  char* device = evt->endpoint_name + strlen(evt->endpoint_name) + 1;
  if (device >= (evt->endpoint_name + sizeof(evt->endpoint_name)))
    device = NULL;

  kprintf("handleDeviceConnected(icm_fr): uuid=%s connection_key=%x connection_id=%x link_info=%x unk1=%x\n  endpoint_name=%s %s\n",
    uuidstr, evt->connection_key & 0xff, evt->connection_id & 0xff, evt->link_info & 0xffff, evt->unknown1 & 0xffff, vendor, device);

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleDeviceConnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }

  // Connected devices' PCIe ports don't appear to get set up until about 300-500ms after we receive the device-connected notification.
  // Queue the rescan 1000ms into the future to try and avoid catching the device before the PCIe ports have been attached.
  // (If we miss it here, we'll get another chance when the IOThunderboltPort services are published, but that'll be after a ~20 second scan delay)
  m_rescanDelayTimer->setTimeoutMS(1000);
}

void IOThunderboltICMListener::handleDeviceDisconnected(icm_fr_event_device_disconnected* evt) {
  kprintf("handleDeviceDisconnected(icm_fr): reserved=%x link_info=%x\n", evt->reserved & 0xffff, evt->link_info & 0xffff);

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleDeviceDisconnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }

  // PCIe teardown seems to happen before the deviceDisconnected message is sent, so we don't need a delay here.
  rescanDSB1();
}

void IOThunderboltICMListener::handleXDomainConnected(icm_fr_event_xdomain_connected* evt) {
  uuid_string_t uuidstr;
  uuid_unparse(evt->remote_uuid, uuidstr);
  kprintf("handleXDomainConnected(icm_fr): remote_uuid=%s\n", uuidstr);
  uuid_unparse(evt->local_uuid, uuidstr);
  kprintf("handleXDomainConnected(icm_fr): local_uuid=%s\n", uuidstr);
  kprintf("handleXDomainConnected(icm_fr): link_info=%x local_route=%08x%08x remote_route=%08x%08x\n", evt->reserved & 0xfff, evt->local_route_hi, evt->local_route_lo, evt->remote_route_hi, evt->remote_route_lo);

  ICMXDomainRegistry* registry = ICMXDomainRegistry::registryForController(m_controller);
  // Notify the XDomain registry that we've learned the local UUID for this domain (which isn't available until the first XDomain connection notification arrives)
  registry->didLearnDomainUUID(evt->local_uuid);

  // Register the new XDomain link
  ICMXDomainRegistryEntry* newLink = ICMXDomainRegistryEntry::withLinkDetails(evt->local_uuid, evt->remote_uuid, combine_route(evt->local_route_hi, evt->local_route_lo), combine_route(evt->remote_route_hi, evt->remote_route_lo));
  registry->registerXDomainLink(newLink);

  // Check for dual-link port pairing
  {

    uint64_t routeString = combine_route(evt->local_route_hi, evt->local_route_lo);
    IOThunderboltSwitch* parentSwitch = m_controller->getRootSwitch();
    IOThunderboltPort* port = NULL;
    for (uint64_t remainingRoute = routeString; remainingRoute; ) {
      uint8_t portIndex = remainingRoute & 0xff;
      remainingRoute = (remainingRoute >> 8);

      port = parentSwitch->portAtIndex(portIndex, false);
      if (!port)
        break;

      if (!remainingRoute)
        break;

      IOThunderboltPort* peerPort = OSDynamicCast(IOThunderboltPort, parentSwitch->findValidClientForPort(port));
      if (!peerPort)
        break;

      kprintf("handleXDomainConnected: parentSwitch=%p remainingRoute=%llx portIndex=%x port=%p peerPort=%p\n", parentSwitch, remainingRoute, portIndex, port, peerPort);

      parentSwitch = peerPort->getSwitch();
      kprintf("handleXDomainConnected: childSwitch=%p\n", parentSwitch);

      if (!parentSwitch) {
        port = NULL;
        break;
      }
    }

    kprintf("handleXDomainConnected: port=%p parentSwitch=%p\n", port, parentSwitch);
    if (!(port && parentSwitch)) {
      kprintf("handleXDomainConnected: port lookup failed\n");
      goto dualLinkSetupFailed;
    }

    IOThunderboltPort* dualLinkPort = port->getDualLinkPort();
    if (dualLinkPort) {
      // Find index for dual-link port
      uint32_t dualLinkPortIndex = 0;
      for (; dualLinkPortIndex < parentSwitch->getMaxPortNumber(); ++dualLinkPortIndex) {
        if (parentSwitch->portAtIndex(dualLinkPortIndex, false) == dualLinkPort)
          break;
      }
      if (dualLinkPortIndex >= parentSwitch->getMaxPortNumber()) {
        kprintf("handleXDomainConnected: Port appears to be a dual-link port, but unable to resolve the paired port index\n");
        goto dualLinkSetupFailed;
      } 

      uint64_t dualLinkRouteString = (dualLinkPortIndex << (parentSwitch->getThunderboltDepth() * 8)) | parentSwitch->getRouteString();
      kprintf("handleXDomainConnected: Registering extra connection for dual-link paired port: route string %llx, index %x\n", dualLinkRouteString, dualLinkPortIndex);

      ICMXDomainRegistryEntry* dualLink = ICMXDomainRegistryEntry::withLinkDetails(evt->local_uuid, evt->remote_uuid, dualLinkRouteString, combine_route(evt->remote_route_hi, evt->remote_route_lo));
      dualLink->m_dualLinkPrimary = newLink;
      newLink->m_dualLinkSecondary = dualLink;
      registry->registerXDomainLink(dualLink);
      OSSafeReleaseNULL(dualLink);
    }
  }

dualLinkSetupFailed:

  OSSafeReleaseNULL(newLink);

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleXDomainConnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }
}

void IOThunderboltICMListener::handleXDomainDisconnected(icm_fr_event_xdomain_disconnected* evt) {
  uuid_string_t uuidstr;
  uuid_unparse(evt->remote_uuid, uuidstr);
  kprintf("handleXDomainDisconnected(icm_fr): remote_uuid=%s link_info=%x\n", uuidstr, evt->link_info & 0xffff);

  ICMXDomainRegistry* registry = ICMXDomainRegistry::registryForController(m_controller);
  ICMXDomainRegistryEntry* oldLink = registry->entryForRemoteUUID(evt->remote_uuid);
  if (oldLink) {
    registry->unregisterXDomainLink(oldLink);
  } else {
    kprintf("handleXDomainDisconnected: link data not found in the XDomain registry for this controller?\n");
  }

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleXDomainDisconnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }
}

