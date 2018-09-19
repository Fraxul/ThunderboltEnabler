#pragma once
#include <IOKit/IOService.h>

class IOThunderboltArray;
class IOThunderboltCommandGate;
class IOThunderboltConnectionManager;
class IOThunderboltControlPath;
class IOThunderboltDispatchContext;
class IOThunderboltDispatchQueue;
class IOThunderboltFrameList;
class IOThunderboltJTAG;
class IOThunderboltLocalNode;
class IOThunderboltNHI;
class IOThunderboltNHIReceiveRing;
class IOThunderboltNHITransmitRing;
class IOThunderboltPacketLoggerNub;
class IOThunderboltPort;
class IOThunderboltReceiveQueue;
class IOThunderboltSet;
class IOThunderboltSwitch;
class IOThunderboltTimerCommandQueue;
class IOThunderboltTransmitQueue;

typedef uint64_t IOThunderboltTimeStamp; // ???

// IOThunderboltController : IOService : IORegistryEntry : OSObject
class IOThunderboltController : public IOService {
  OSDeclareDefaultStructors(IOThunderboltController);
public:

  // IOService overrides
  virtual void systemWillShutdown(IOOptionBits) override;
  virtual bool start(IOService*) override;
  virtual bool finalize(IOOptionBits) override;
  virtual IOReturn setPowerState(unsigned long, IOService *) override;

  // IOThunderboltController

  virtual IOReturn setupPowerManagement();
  virtual IOReturn destroyPowerManagement();
  virtual void incrementScanCount();
  virtual void decrementScanCount();
  virtual IOReturn poweredStart();
  virtual void shutdown();
  virtual bool init(IOThunderboltNHI *);
  virtual IOThunderboltCommandGate* getGate();
  virtual void allocateTransmitRing(IOThunderboltNHITransmitRing **,IOThunderboltTransmitQueue *);
  virtual IOReturn deallocateTransmitRing(IOThunderboltNHITransmitRing *);
  virtual void allocateReceiveRing(IOThunderboltNHIReceiveRing **,IOThunderboltReceiveQueue *);
  virtual IOReturn deallocateReceiveRing(IOThunderboltNHIReceiveRing *);
  virtual IOThunderboltConnectionManager* getConnectionManager();
  virtual IOThunderboltControlPath* getControlPath();
  virtual IOThunderboltTimerCommandQueue* getTimerQueue();
  virtual void reprobePCI();
  virtual IOThunderboltLocalNode* getLocalNode();
  virtual void registerPacketLogger(IOThunderboltPacketLoggerNub *);
  virtual void unregisterPacketLogger(IOThunderboltPacketLoggerNub *);
  virtual IOThunderboltPacketLoggerNub* getPacketLogger();
  virtual int getThunderboltBootArgs();
  virtual void sleep();
  virtual void wake();
  virtual void earlyWake();
  virtual void lateSleep();
  virtual void updateDevicesConnectedState();
  virtual IOThunderboltSwitch* switchForRouteString(unsigned long long);
  virtual IOThunderboltSwitch* switchForUID(unsigned long long);
  virtual IOThunderboltSwitch* getRootSwitch();
  virtual IOThunderboltPort* getNHIPort();
  virtual bool inQuiesceMode();
  virtual void setQuiesceMode(bool);
  virtual IOThunderboltSet* getSwitchesAtDepth(unsigned int);
  virtual IOReturn configReadDWord(unsigned long long,unsigned int,unsigned int,unsigned int,unsigned int *);
  virtual IOReturn configWriteDWord(unsigned long long,unsigned int,unsigned int,unsigned int,unsigned int);
  virtual IOReturn configModifyDWordWithMask(unsigned long long,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int);
  virtual IOReturn configPollDWordWithMask(unsigned long long,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int);
  virtual void incrementGeneration(bool);
  virtual IOReturn findCapability(unsigned long long,unsigned int,unsigned int,unsigned int,unsigned int,unsigned int *);
  virtual void* getStatistics();
  virtual int getSupportedLinkWidth();
  virtual void treeApply(void (OSMetaClassBase::*)(),void *,void *,void *,void *);
  virtual bool needsEarlyWake();
  virtual IOThunderboltDispatchQueue* getControlPathDispatchQueue();
  virtual void updateDevicesConnectedStateContext(IOThunderboltDispatchContext *);
  virtual void forceInUseState();
  virtual void platformResetComplete();
  virtual IOThunderboltDispatchQueue* getGlobalDispatchQueue();
  virtual IOThunderboltDispatchQueue* getPathManagerDispatchQueue();
  virtual IOReturn enableSleep(bool);
  virtual void processPlugEvent();
  virtual void setUSBMode(bool);
  virtual IOReturn setupJTAG();
  virtual void destroyJTAG();
  virtual int getJTAGDeviceCount();
  virtual IOReturn setTMS(unsigned char,bool);
  virtual IOReturn setTCK(bool);
  virtual IOReturn setTDI(bool);
  virtual IOReturn getTDO(bool *);
  virtual IOThunderboltJTAG* getJTAGInterface();
  virtual IOThunderboltArray* getSwitchesForRoute(unsigned long long);
  virtual void* copyMapper();
  virtual void _RESERVEDIOThunderboltController0();
  virtual void _RESERVEDIOThunderboltController1();
  virtual void _RESERVEDIOThunderboltController2();
  virtual void _RESERVEDIOThunderboltController3();
  virtual void _RESERVEDIOThunderboltController4();
  virtual void _RESERVEDIOThunderboltController5();
  virtual void _RESERVEDIOThunderboltController6();
  virtual void _RESERVEDIOThunderboltController7();
};

