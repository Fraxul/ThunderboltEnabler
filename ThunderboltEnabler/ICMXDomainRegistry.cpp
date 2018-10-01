#include "ICMXDomainRegistry.h"
#include "IOThunderboltController.h"
#include "IOThunderboltLocalNode.h"
#include <IOKit/IOLocks.h>

// Defined in IOThunderboltFamily
extern "C" {
  OSObject* _ZN19IOThunderboltString11withCStringEPKc(const char*);
};

static OSArray* s_controllerRegistry = nullptr;
static IOSimpleLock* s_controllerRegistryLock = nullptr;

OSDefineMetaClassAndStructors(ICMXDomainRegistry, OSArray);

/*static*/ void ICMXDomainRegistry::staticInit() {
  s_controllerRegistryLock = IOSimpleLockAlloc();
  s_controllerRegistry = OSArray::withCapacity(4);
}

/*static*/ ICMXDomainRegistry* ICMXDomainRegistry::registryForController(IOThunderboltController* controller) {
  ICMXDomainRegistry* res = NULL;

  IOSimpleLockLock(s_controllerRegistryLock);

  // Simple linear scan of the controller-registry mapping
  for (uint32_t i = 0; i < s_controllerRegistry->getCount(); ++i) {
    ICMXDomainRegistry* obj = OSDynamicCast(ICMXDomainRegistry, s_controllerRegistry->getObject(i));
    if (!obj)
      continue;

    if (obj->m_controller == controller) {
      // Found a match
      res = obj;
      break;
    }
  }

  if (!res) {
    // No registry found for this controller, so create one
    res = OSTypeAlloc(ICMXDomainRegistry);
    assert(res);
    res->initWithCapacity(4);
    res->m_controller = controller;
    res->m_ipService = NULL;

    s_controllerRegistry->setObject(res);
    // OSArray adds a ref, so release ours
    res->release();
  }

  IOSimpleLockUnlock(s_controllerRegistryLock);
  return res;
}

/*static*/ void ICMXDomainRegistry::willRetireController(IOThunderboltController* controller) {
  IOSimpleLockLock(s_controllerRegistryLock);

  // Delete the registry for this controller, if one exists
  for (uint32_t i = 0; i < s_controllerRegistry->getCount(); ++i) {
    ICMXDomainRegistry* obj = OSDynamicCast(ICMXDomainRegistry, s_controllerRegistry->getObject(i));
    if (!obj)
      continue;

    if (obj->m_controller == controller) {
      // Found a match
      s_controllerRegistry->removeObject(i);
      break;
    }
  }

  IOSimpleLockUnlock(s_controllerRegistryLock);
}

void ICMXDomainRegistry::registerXDomainLink(ICMXDomainRegistryEntry* entry) {
  setObject(entry);
}

ICMXDomainRegistryEntry* ICMXDomainRegistry::entryForRouteString(uint64_t localRoute) {
  for (uint32_t i = 0; i < getCount(); ++i) {
    ICMXDomainRegistryEntry* obj = OSDynamicCast(ICMXDomainRegistryEntry, getObject(i));
    assert(obj);
    if (obj->m_localRoute == localRoute) {
      // Prioritize returning the primary for dual-link pairs
      if (obj->m_dualLinkPrimary)
        return obj->m_dualLinkPrimary;

      return obj;
    }
  }
  return NULL;
}

ICMXDomainRegistryEntry* ICMXDomainRegistry::entryForRemoteUUID(uuid_t u) { 
  for (uint32_t i = 0; i < getCount(); ++i) {
    ICMXDomainRegistryEntry* obj = OSDynamicCast(ICMXDomainRegistryEntry, getObject(i));
    assert(obj);
    if (uuid_compare(obj->m_remoteUUID, u) == 0) {
      // Prioritize returning the primary for dual-link pairs
      if (obj->m_dualLinkPrimary)
        return obj->m_dualLinkPrimary;

      return obj;
    }
  }
  return NULL;
}
void ICMXDomainRegistry::unregisterXDomainLink(ICMXDomainRegistryEntry* entry) {
  if (!entry)
    return;

  for (uint32_t i = 0; i < getCount(); ++i) {
    ICMXDomainRegistryEntry* obj = OSDynamicCast(ICMXDomainRegistryEntry, getObject(i));
    if (obj == entry) {
      // Save the dual-link entry before we call removeObject, since the object should be destroyed then.
      ICMXDomainRegistryEntry* dualLink = obj->m_dualLinkPrimary ?: obj->m_dualLinkSecondary;
      removeObject(i);

      if (dualLink) {
        // Unregister the other half of this dual-link entry
        unregisterXDomainLink(dualLink);
      }

      return;
    }
  }
}

void ICMXDomainRegistry::setIPService(AppleThunderboltIPService* ipService) {
  if (m_ipService && m_ipService != ipService) {
    kprintf("ICMXDomainRegistry::setIPService: WARNING: This domain already has an IP service set (%p, new is %p)?\n", m_ipService, ipService);
  }
  m_ipService = ipService;

  // Just in case we somehow missed patching the UUID on the IP service before, patch it now.
  // This is harmless if we haven't learned the correct domain UUID yet.
  applyIPServiceUUIDPatch();
}

void ICMXDomainRegistry::didLearnDomainUUID(uuid_t newLocalUUID) {
  uuid_t localNode_old_uuid;
  IOThunderboltLocalNode* localNode = m_controller->getLocalNode();
  localNode->getDomainUUID(localNode_old_uuid);
  if (uuid_compare(localNode_old_uuid, newLocalUUID) == 0)
    return; // UUID already up-to-date

  // Patch the IOThunderboltLocalNode if its UUID doesn't match the local_uuid in the connection packet.
  // We don't have any way to set the local node UUID correctly before receiving the first XDomain connection.
  uuid_string_t localNode_old_uuidstr, newLocalUUIDstr;

  uuid_unparse(localNode_old_uuid, localNode_old_uuidstr);
  uuid_unparse(newLocalUUID, newLocalUUIDstr);
  kprintf("ICMXDomainRegistry::didLearnDomainUUID: IOThunderboltLocalNode's UUID (%s) is being replaced with learned UUID %s\n", localNode_old_uuidstr, newLocalUUIDstr);

  // Patch the object member variable
  uint8_t* localNode_uuid_to_patch = (reinterpret_cast<uint8_t*>(localNode) + 0xa8);
  memcpy(localNode_uuid_to_patch, newLocalUUID, 16);

  // Fixup the IORegistryEntry "Domain UUID" property on the IOThunderboltLocalNode
  OSObject* uuidProp = _ZN19IOThunderboltString11withCStringEPKc(newLocalUUIDstr);
  localNode->setProperty("Domain UUID", uuidProp);
  OSSafeReleaseNULL(uuidProp);

  // Apply the patch to the IP service. We should have a pointer to it by now.
  applyIPServiceUUIDPatch();
}

void ICMXDomainRegistry::applyIPServiceUUIDPatch() {
  if (!m_ipService)
    return;

  kprintf("ICMXDomainRegistry::applyIPServiceUUIDPatch(): patching AppleThunderboltIPService at %p\n", m_ipService);

  uint8_t* ipService_uuid_to_patch = (reinterpret_cast<uint8_t*>(m_ipService) + 0xe0);
  IOThunderboltLocalNode* localNode = m_controller->getLocalNode();
  localNode->getDomainUUID(ipService_uuid_to_patch);
}


OSDefineMetaClassAndStructors(ICMXDomainRegistryEntry, OSObject);

bool ICMXDomainRegistryEntry::init() {
  if (!super::init())
    return false;

  memset(&m_localUUID, 0, sizeof(uuid_t));
  memset(&m_remoteUUID, 0, sizeof(uuid_t));
  m_localRoute = 0;
  m_remoteRoute = 0;
  m_dualLinkPrimary = NULL;
  m_dualLinkSecondary = NULL;
  return true;
}

/*static*/ ICMXDomainRegistryEntry* ICMXDomainRegistryEntry::withLinkDetails(uuid_t localUUID, uuid_t remoteUUID, uint64_t localRoute, uint64_t remoteRoute) {
  ICMXDomainRegistryEntry* res = OSTypeAlloc(ICMXDomainRegistryEntry);
  if (!res)
    return NULL;

  if (!res->init()) {
    OSSafeReleaseNULL(res);
    return NULL;
  }

  uuid_copy(res->m_localUUID, localUUID);
  uuid_copy(res->m_remoteUUID, remoteUUID);
  res->m_localRoute = localRoute;
  res->m_remoteRoute = remoteRoute;
  return res;
}
