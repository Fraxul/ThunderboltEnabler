#pragma once
#include <libkern/c++/OSObject.h>

class IOThunderboltTimerCommand;
class IOThunderboltTransmitCommand;
class IOThunderboltReceiveCommand;
class IOThunderboltConfigCommand;
class IOThunderboltController;
class IOThunderboltControlPathListener;

class IOThunderboltControlPath : public OSObject {
  OSDeclareDefaultStructors(IOThunderboltControlPath);
public:
  virtual void timerCommandCallback(void *,int,IOThunderboltTimerCommand *);
  virtual void txCommandCallback(void *,int,IOThunderboltTransmitCommand *);
  virtual void rxCommandCallback(void *,int,IOThunderboltReceiveCommand *);
  virtual void createTimers();
  virtual void newTimerCommand();
  virtual void destroyTimers();
  virtual void createTransmitter();
  virtual void newTransmitCommand();
  virtual void destroyTransmitter();
  virtual void createReceiver();
  virtual void newReceiveCommand();
  virtual void destroyReceiver();
  virtual void resubmitCommand(IOThunderboltConfigCommand *);
  virtual void completeCommand(IOThunderboltConfigCommand *,int);
  virtual void submitWorkToTransmitter();
  virtual void submitRequestQueueWorkToTransmitter();
  virtual void submitResponseQueueWorkToTransmitter();
  virtual void submit(IOThunderboltConfigCommand *);
  virtual void sleep();
  virtual void wake();
  virtual void initWithController(IOThunderboltController *);
  virtual void start();
  virtual void stop();
  virtual void isStarted();
  virtual IOReturn addListener(IOThunderboltControlPathListener *);
  virtual IOReturn removeListener(IOThunderboltControlPathListener *);
  virtual void fakePlugEvent(bool,unsigned long long,unsigned int);
  virtual void completeAllCommands(int);
  virtual void getNextSequenceNumberForPDF(unsigned char);
  virtual void _RESERVEDIOThunderboltControlPath0();
  virtual void _RESERVEDIOThunderboltControlPath1();
  virtual void _RESERVEDIOThunderboltControlPath2();
  virtual void _RESERVEDIOThunderboltControlPath3();
  virtual void _RESERVEDIOThunderboltControlPath4();
  virtual void _RESERVEDIOThunderboltControlPath5();
  virtual void _RESERVEDIOThunderboltControlPath6();
  virtual void _RESERVEDIOThunderboltControlPath7();
  virtual void _RESERVEDIOThunderboltControlPath8();
  virtual void _RESERVEDIOThunderboltControlPath9();
  virtual void _RESERVEDIOThunderboltControlPath10();
  virtual void _RESERVEDIOThunderboltControlPath11();
  virtual void _RESERVEDIOThunderboltControlPath12();
  virtual void _RESERVEDIOThunderboltControlPath13();
  virtual void _RESERVEDIOThunderboltControlPath14();
  virtual void _RESERVEDIOThunderboltControlPath15();
};

