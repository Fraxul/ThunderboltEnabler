#pragma once
#include <IOKit/IOService.h>

class IOInterruptEventSource;
class IOPCIDevice;

class AppleThunderboltGenericHAL : public IOService {
  OSDeclareDefaultStructors(AppleThunderboltGenericHAL);
public:

  // OSObject
  virtual void free(void) override;

  // IOService
  virtual void systemWillShutdown(IOOptionBits) override;
  virtual IOService* probe(IOService *,int *) override;
  virtual bool start(IOService *) override;
  virtual void stop(IOService *) override;
  virtual bool finalize(IOOptionBits) override;
  virtual IOWorkLoop* getWorkLoop() const override;
  virtual IOReturn callPlatformFunction( const OSSymbol * functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) override;
  virtual IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice ) override;

  // AppleThunderboltGenericHAL

  virtual void startNHI();
  virtual void stopNHI();
  virtual void isNHIStarted();
  virtual void registerInterruptCallback(OSObject *,void (*)(OSObject *));
  virtual uint32_t configRead32(unsigned char address);
  virtual uint32_t configWrite32(unsigned char address, uint32_t value);
  virtual uint16_t configRead16(unsigned char address);
  virtual uint16_t configWrite16(unsigned char address, uint16_t value);
  virtual uint8_t configRead8(unsigned char address);
  virtual uint8_t configWrite8(unsigned char address, uint8_t value);
  virtual uint32_t registerRead32(uint32_t address);
  virtual uint32_t registerWrite32(uint32_t address, uint32_t value);
  virtual uint16_t registerRead16(uint32_t address);
  virtual uint16_t registerWrite16(uint32_t address, uint16_t value);
  virtual uint8_t registerRead8(uint32_t address);
  virtual uint8_t registerWrite8(uint32_t address, uint8_t value);
  virtual void setupExclusiveAccess();
  virtual void destroyExclusiveAccess();
  virtual void acquireHardware(IOService *);
  virtual void releaseHardware(IOService *);
  virtual void createMemoryBlock(unsigned long,unsigned int);
  virtual void reprobePCI();
  virtual void registerPMCallback(OSObject *,void (*)(OSObject *,unsigned long));
  virtual void mmioWorkaround();
  virtual void enableConfigAccess(bool);
  virtual void getPowerState();
  virtual void getBootArgs();
  virtual IOPCIDevice* getPCIDevice();
  virtual void forcePowerState(unsigned long);
  virtual void enableDeepSleep(bool);
  virtual void isConfigAccessEnabled();
  virtual void poweredStart();
  virtual void createNHI();
  virtual void setupPowerManagement();
  virtual void destroyPowerManagement();
  virtual void lateSleep();
  virtual void prePCIWake();
  virtual void earlyWake();
  virtual void handleInterrupt(IOInterruptEventSource *,int);
  virtual void configHandler(unsigned int,IOPCIDevice *,unsigned int);
  virtual void rootConfigHandler(unsigned int,IOPCIDevice *,unsigned int);
  virtual void getStatistics();
  virtual void halPMStateIOKitPMState(unsigned int);
  virtual void iokitPMStateToHALPMState(unsigned int);
  virtual void isDuringEarlyWake();
  virtual void getParentBridgeDriver();
  virtual IOPCIDevice* getParentBridgeDevice();
  virtual void getRootBridgeDriver();
  virtual IOPCIDevice* getRootBridgeDevice();
  virtual void getRootPortDriver();
  virtual IOPCIDevice* getRootPortDevice();
  virtual void copyMapper();
};

