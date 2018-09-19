#pragma once
#include "IOThunderboltConnectionManager.h"

class IOThunderboltICM : public IOThunderboltConnectionManager {
  OSDeclareDefaultStructors(IOThunderboltICM);
public:
  virtual void free() override;

/*
  virtual void createResources() override;
  virtual void destroyResources() override;
  virtual void configurationThreadDone() override;
  virtual void configurationThreadMain() override;
  virtual void resetRootSwitch() override;
  virtual void scanRootSwitch(unsigned int) override;
  virtual void scan(void *) override;
  virtual void earlyWakeScan(void *) override;
  virtual void wakeScan(void *) override;
  virtual void rescan(void *) override;
  virtual bool initWithController(IOThunderboltController *) override;
  virtual IOThunderboltSwitch* getRootSwitch() override;
  virtual IOThunderboltPort* getNHIPort() override;
  virtual bool isConfigurationThreadRunning() override;
  virtual void startConfigurationThread(IOThunderboltConnectionManager::Callback) override;
  virtual void startScan() override;
  virtual void startEarlyWakeScan() override;
  virtual void startWakeScan() override;
  virtual void lateSleep() override;
  virtual void lateSleepPhase2() override;
  virtual void startRescan() override;
  virtual void appendSwitchToScanQueue(IOThunderboltSwitch *) override;
  virtual void resetAll() override;
  virtual void getDevicesConnectedState(bool *) override;
  virtual void createRootSwitch() override;
  virtual void terminateAndRegister() override;
  virtual void disableConfigurationThread() override;
  virtual void enableConfigurationThread() override;
*/
protected:
  IOThunderboltController* m_controller;

};

