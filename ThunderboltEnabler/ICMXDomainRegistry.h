#pragma once
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>

class IOThunderboltController;
class ICMXDomainRegistryEntry;

class ICMXDomainRegistry : public OSArray {
  OSDeclareDefaultStructors(ICMXDomainRegistry);
public:
  static void staticInit();
  static ICMXDomainRegistry* registryForController(IOThunderboltController*);
  static void willRetireController(IOThunderboltController*);

  virtual void registerXDomainLink(ICMXDomainRegistryEntry*); // Adds ref
  virtual void unregisterXDomainLink(ICMXDomainRegistryEntry*);
  virtual ICMXDomainRegistryEntry* entryForRouteString(uint64_t localRoute);
  virtual ICMXDomainRegistryEntry* entryForRemoteUUID(uuid_t);

protected:
  IOThunderboltController* m_controller;
};

class ICMXDomainRegistryEntry : public OSObject {
  OSDeclareDefaultStructors(ICMXDomainRegistryEntry);
public:
  typedef OSObject super;
  static ICMXDomainRegistryEntry* withLinkDetails(uuid_t localUUID, uuid_t remoteUUID, uint64_t localRoute, uint64_t remoteRoute);

  virtual bool init() override;

  uuid_t m_localUUID;
  uuid_t m_remoteUUID;
  uint64_t m_localRoute;
  uint64_t m_remoteRoute;
  ICMXDomainRegistryEntry* m_dualLinkPrimary;
  ICMXDomainRegistryEntry* m_dualLinkSecondary;
};

