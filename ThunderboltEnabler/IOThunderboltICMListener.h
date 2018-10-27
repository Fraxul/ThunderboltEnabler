#pragma once
#include "IOThunderboltControlPathListener.h"
class IONotifier;
class IOPCIDevice;
class IOService;
class IOTimerEventSource;

struct icm_fr_event_device_connected;
struct icm_fr_event_device_disconnected;
struct icm_fr_event_xdomain_connected;
struct icm_fr_event_xdomain_disconnected;

class IOThunderboltICMListener : public IOThunderboltControlPathListener {
  OSDeclareDefaultStructors(IOThunderboltICMListener);
public:
  static IOThunderboltICMListener* withController(IOThunderboltController*);

  virtual void processResponse(IOThunderboltReceiveCommand*) override;
  virtual bool initWithController(IOThunderboltController*) override;
  virtual void free() override;

protected:
  IOThunderboltController* m_controller;
  IONotifier* m_portPublishedNotification;
  IONotifier* m_portTerminatedNotification;
  IOTimerEventSource* m_rescanDelayTimer;
  IOPCIDevice* m_dsb1;

  bool handleThunderboltPortPublishedNotification(void* refCon, IOService*, IONotifier*);
  bool handleThunderboltPortTerminatedNotification(void* refCon, IOService*, IONotifier*);
  void delayedRescanTimerFired(OSObject* owner, IOTimerEventSource*);
  void rescanDSB1();

  void handleDeviceConnected(icm_fr_event_device_connected*);
  void handleDeviceDisconnected(icm_fr_event_device_disconnected*);
  void handleXDomainConnected(icm_fr_event_xdomain_connected*);
  void handleXDomainDisconnected(icm_fr_event_xdomain_disconnected*);
};

