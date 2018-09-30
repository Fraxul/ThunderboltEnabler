#pragma once
#include "IOThunderboltNub.h"
class IOThunderboltSwitch;
class IOThunderboltPath;

typedef size_t IOThunderboltLane; // ???

class IOThunderboltPort : public IOThunderboltNub {
  OSDeclareDefaultStructors(IOThunderboltPort);
public:
  typedef uint32_t PortState; // ???, should be an enum

  // OSObject
  virtual void free(void) override;

  // IOService
  virtual bool finalize(IOOptionBits) override;
  virtual bool matchPropertyTable(OSDictionary *) override;

  // IOThunderboltNub
  virtual bool initWithController(IOThunderboltController *) override;
  virtual unsigned int getThunderboltDepth() override;
  virtual uint64_t getRouteString() override;

  // IOThunderboltPort
  virtual void setSwitch(IOThunderboltSwitch *);
  virtual void setUpstream(bool);
  virtual void setMaxInHopID(unsigned short);
  virtual void setMaxOutHopID(unsigned short);
  virtual void allocateNonFlowControlledCredits(unsigned int);
  virtual void deallocateNonFlowControlledCredits(unsigned int);
  virtual void addPathToActivePaths(IOThunderboltPath *);
  virtual void removePathFromActivePaths(IOThunderboltPath *);
  virtual void setPeerPort(IOThunderboltPort*);
  virtual void linkPorts(IOThunderboltPort*);
  virtual void setMicro(IOThunderboltSwitch *,unsigned char,unsigned char);
  virtual void setTerminated();
  virtual void setInvalid();
  virtual void ensureRegistered();
  virtual void processScanResults(unsigned int *);
  virtual uint32_t getAdapterType();
  virtual void getInHopSet();
  virtual void getMaxInHopID();
  virtual void getOutHopSet();
  virtual void getMaxOutHopID();
  virtual void getMaxCounters();
  virtual IOThunderboltSwitch* getSwitch();
  virtual bool isUpstream();
  virtual uint32_t getPortNumber();
  virtual IOThunderboltSwitch* getParentSwitch();
  virtual IOThunderboltPort* getChildPort();
  virtual void* getChildXDomainLink();
  virtual IOThunderboltPort* getParentPort();
  virtual IOThunderboltSwitch* getChildSwitch();
  virtual void getActivePaths();
  virtual IOThunderboltPort* getPeerPort();
  virtual void cmReadDWord(unsigned char,unsigned int *);
  virtual void getMicro(IOThunderboltSwitch **,unsigned char *,unsigned char *);
  virtual bool isTerminated();
  virtual bool isInvalid();
  virtual void getHardwareBandwidth();
  virtual void getRequiredBandwidthAllocated();
  virtual void getMaximumBandwidthAllocated();
  virtual void getPortAffinity();
  virtual void enableTimeSyncMasterSynchronous(bool);
  virtual void enableTimeSyncSlaveSynchronous(bool);
  virtual void allocateBandwidth(unsigned int,unsigned int);
  virtual void deallocateBandwidth(unsigned int,unsigned int);
  virtual IOThunderboltLane getLane();
  virtual IOThunderboltPort* getDualLinkPort();
  virtual void setPortAffinity(unsigned int);
  virtual void setLane(IOThunderboltLane);
  virtual void setDualLinkPort(IOThunderboltPort*);
  virtual void clearPortAffinity();
  virtual void setPhyConfigOffset(unsigned int);
  virtual void disableLink(bool);
  virtual bool isLinkDisabled();
  virtual uint32_t getPhyConfigOffset();
  virtual void lowSpeedPeerPort();
  virtual void setHardwareBandwidth(unsigned int);
  virtual void setLastNegotiatedLinkWidth(unsigned int);
  virtual void getLastNegotiatedLinkWidth();
  virtual PortState getPortState();
  virtual void setPortState(IOThunderboltPort::PortState);
  virtual void getMaxCredits();
  virtual void configureSharedBuffers(bool);
  virtual void getHPM(IOThunderboltSwitch **,unsigned char *,unsigned char *);
  virtual void setHPM(IOThunderboltSwitch *,unsigned char,unsigned char);
  virtual void getTotalBuffers();
  virtual void _RESERVEDIOThunderboltPort0();
  virtual void _RESERVEDIOThunderboltPort1();
  virtual void _RESERVEDIOThunderboltPort2();
  virtual void _RESERVEDIOThunderboltPort3();
  virtual void _RESERVEDIOThunderboltPort4();
  virtual void _RESERVEDIOThunderboltPort5();
  virtual void _RESERVEDIOThunderboltPort6();
  virtual void _RESERVEDIOThunderboltPort7();
};

