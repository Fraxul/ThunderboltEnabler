#include <libkern/c++/OSObject.h>

class IOThunderboltController;
class IOThunderboltSwitch;
class IOThunderboltPort;


class IOThunderboltICM : public OSObject {
public:
  typedef void* Callback;
  // Don't touch the ordering of these or insert any functions here -- the vtable needs to
  // match the layout of IOThunderboltConnectionManager.
  OSDeclareDefaultStructors(IOThunderboltICM);
  virtual void free() override;

  virtual void createResources();
  virtual void destroyResources();
  virtual void configurationThreadDone();
  virtual void configurationThreadMain();
  virtual void resetRootSwitch();
  virtual void scanRootSwitch(unsigned int);
  virtual void scan(void *);
  virtual void earlyWakeScan(void *);
  virtual void wakeScan(void *);
  virtual void rescan(void *);
  virtual bool initWithController(IOThunderboltController *);
  virtual IOThunderboltSwitch* getRootSwitch();
  virtual IOThunderboltPort* getNHIPort();
  virtual bool isConfigurationThreadRunning();
  virtual void startConfigurationThread(IOThunderboltICM::Callback);
  virtual void startScan();
  virtual void startEarlyWakeScan();
  virtual void startWakeScan();
  virtual void lateSleep();
  virtual void lateSleepPhase2();
  virtual void startRescan();
  virtual void appendSwitchToScanQueue(IOThunderboltSwitch *);
  virtual void resetAll();
  virtual void getDevicesConnectedState(bool *);
  virtual void createRootSwitch();
  virtual void terminateAndRegister();
  virtual void disableConfigurationThread();
  virtual void enableConfigurationThread();

  virtual void _RESERVEDIOThunderboltConnectionManager0(void);
  virtual void _RESERVEDIOThunderboltConnectionManager1(void);
  virtual void _RESERVEDIOThunderboltConnectionManager2(void);
  virtual void _RESERVEDIOThunderboltConnectionManager3(void);
  virtual void _RESERVEDIOThunderboltConnectionManager4(void);
  virtual void _RESERVEDIOThunderboltConnectionManager5(void);
  virtual void _RESERVEDIOThunderboltConnectionManager6(void);
  virtual void _RESERVEDIOThunderboltConnectionManager7(void);
  // Adding functions below here should be safe.

protected:
  IOThunderboltController* m_controller;

};

