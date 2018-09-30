#pragma once
#include <IOKit/IOService.h>
class IOThunderboltController;
class IOThunderboltReceiveCommand;
class IOThunderboltXDomainLink;
class IOThunderboltXDLocalPropertiesCache;
class IOThunderboltConfigCommand;
class IOThunderboltDispatchContext;

class IOThunderboltLocalNode : public IOService {
  OSDeclareDefaultStructors(IOThunderboltLocalNode);
public:

  // OSObject
  virtual void free() override;

  // IOService
  virtual bool finalize(IOOptionBits) override;
  virtual IOReturn message(unsigned int,IOService *,void *) override;

  // IOThunderboltLocalNode
  virtual void createResources();
  virtual void destroyResources();
  virtual void createResponseCommands();
  virtual void newXDomainResponseCommand();
  virtual void destroyResponseCommands();
  virtual void sendROMChangedNotifications();
  virtual void processROMChangedPacket(unsigned char *,unsigned long long);
  virtual void initWithController(IOThunderboltController *);
  virtual void listenerCallback(void *,IOThunderboltReceiveCommand *);
  virtual void getDomainUUID(unsigned char *);
  virtual void toggleROM();
  virtual void registerXDomainLink(IOThunderboltXDomainLink *);
  virtual void unregisterXDomainLink(IOThunderboltXDomainLink *);
  virtual void findXDomainLink(unsigned char *);
  virtual void createPropertiesCacheCopy();
  virtual void setPropertiesCache(IOThunderboltXDLocalPropertiesCache *);
  virtual void setPropertiesBytes(void const*,unsigned int);
  virtual IOThunderboltController* getController();
  virtual void configCommandCallback(void *,int,IOThunderboltConfigCommand *);
  virtual void publishXDService(char *,unsigned int,unsigned int,unsigned int,unsigned int);
  virtual void unpublishXDService(char *);
  virtual void linkStateChange(IOThunderboltDispatchContext *);
  virtual void _RESERVEDIOThunderboltLocalNode0();
  virtual void _RESERVEDIOThunderboltLocalNode1();
  virtual void _RESERVEDIOThunderboltLocalNode2();
  virtual void _RESERVEDIOThunderboltLocalNode3();
  virtual void _RESERVEDIOThunderboltLocalNode4();
  virtual void _RESERVEDIOThunderboltLocalNode5();
  virtual void _RESERVEDIOThunderboltLocalNode6();
  virtual void _RESERVEDIOThunderboltLocalNode7();
};

