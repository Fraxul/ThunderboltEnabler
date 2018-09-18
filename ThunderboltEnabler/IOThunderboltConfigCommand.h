#pragma once
#include <libkern/c++/OSObject.h>

#include "IOThunderboltCommand.h"

class IOThunderboltTransmitCommand;
class IOThunderboltReceiveCommand;

class IOThunderboltFrameList;
typedef uint64_t IOThunderboltTimeStamp; // ???


class IOThunderboltConfigCommand : public IOThunderboltCommand {
  OSDeclareDefaultStructors(IOThunderboltConfigCommand);
public:
  typedef void* Completion;
  // Don't touch the ordering of these or insert any functions here -- the vtable needs to
  // match the layout of IOThunderboltConfigCommand in IOThunderboltFamily.kext.
  virtual bool initWithController(IOThunderboltController *) override;

  virtual void setInProgress(bool);
  virtual int prepareForExecution(IOThunderboltTransmitCommand *);
  virtual int processRequestComplete(IOThunderboltTransmitCommand *);
  virtual int processResponse(IOThunderboltReceiveCommand *);
  virtual void complete(int);
  virtual void getConfigQueue(void);
  virtual void createResources(void);
  virtual void destroyResources(void);
  virtual void setCompletion(IOThunderboltConfigCommand::Completion);
  virtual void getCompletion(void);
  virtual void setRouteString(uint64_t);
  virtual uint64_t getRouteString(void);
  virtual void isInProgress(void);
  virtual void submit(void);
  virtual int submitSynchronous(void);
  virtual void getTimeout(void);
  virtual void setTimeout(unsigned long long);
  virtual uint8_t getRetries(void);
  virtual void setRetries(uint8_t);
  virtual uint8_t getRemainingRetries(void);
  virtual void setRemainingRetries(uint8_t);
  virtual void _RESERVEDIOThunderboltConfigCommand0(void);
  virtual void _RESERVEDIOThunderboltConfigCommand1(void);
  virtual void _RESERVEDIOThunderboltConfigCommand2(void);
  virtual void _RESERVEDIOThunderboltConfigCommand3(void);
  virtual void _RESERVEDIOThunderboltConfigCommand4(void);
  virtual void _RESERVEDIOThunderboltConfigCommand5(void);
  virtual void _RESERVEDIOThunderboltConfigCommand6(void);
  virtual void _RESERVEDIOThunderboltConfigCommand7(void);
  virtual void _RESERVEDIOThunderboltConfigCommand8(void);
  virtual void _RESERVEDIOThunderboltConfigCommand9(void);
  virtual void _RESERVEDIOThunderboltConfigCommand10(void);
  virtual void _RESERVEDIOThunderboltConfigCommand11(void);
  virtual void _RESERVEDIOThunderboltConfigCommand12(void);
  virtual void _RESERVEDIOThunderboltConfigCommand13(void);
  virtual void _RESERVEDIOThunderboltConfigCommand14(void);
  virtual void _RESERVEDIOThunderboltConfigCommand15(void);
  virtual void _RESERVEDIOThunderboltConfigCommand16(void);
  virtual void _RESERVEDIOThunderboltConfigCommand17(void);
  virtual void _RESERVEDIOThunderboltConfigCommand18(void);
  virtual void _RESERVEDIOThunderboltConfigCommand19(void);
  virtual void _RESERVEDIOThunderboltConfigCommand20(void);
  virtual void _RESERVEDIOThunderboltConfigCommand21(void);
  virtual void _RESERVEDIOThunderboltConfigCommand22(void);
  virtual void _RESERVEDIOThunderboltConfigCommand23(void);
  virtual void _RESERVEDIOThunderboltConfigCommand24(void);
  virtual void _RESERVEDIOThunderboltConfigCommand25(void);
  virtual void _RESERVEDIOThunderboltConfigCommand26(void);
  virtual void _RESERVEDIOThunderboltConfigCommand27(void);
  virtual void _RESERVEDIOThunderboltConfigCommand28(void);
  virtual void _RESERVEDIOThunderboltConfigCommand29(void);
  virtual void _RESERVEDIOThunderboltConfigCommand30(void);
  virtual void _RESERVEDIOThunderboltConfigCommand31(void);
};

