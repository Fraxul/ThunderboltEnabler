#pragma once
#include "IOThunderboltControlPathListener.h"

struct icm_fr_event_device_connected;
struct icm_fr_event_device_disconnected;

class IOThunderboltICMListener : public IOThunderboltControlPathListener {
  OSDeclareDefaultStructors(IOThunderboltICMListener);
public:
  static IOThunderboltICMListener* withController(IOThunderboltController*);

  virtual void processResponse(IOThunderboltReceiveCommand*) override;
  virtual bool initWithController(IOThunderboltController*) override;

protected:
  IOThunderboltController* m_controller;


  void handleDeviceConnected(icm_fr_event_device_connected*);
  void handleDeviceDisconnected(icm_fr_event_device_disconnected*);
};

