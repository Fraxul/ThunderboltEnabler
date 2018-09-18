#pragma once
#include "IOThunderboltCommand.h"
class IOMemoryDescriptor;

class IOThunderboltReceiveCommand : public IOThunderboltCommand {
  OSDeclareDefaultStructors(IOThunderboltReceiveCommand);
public:
  typedef void* Completion;

  virtual bool initWithController(IOThunderboltController *) override;

  virtual void setInProgress(bool);
  virtual bool isInProgress();
  virtual void setEOF(uint8_t);
  virtual void setSOF(uint8_t);
  virtual void setIsochReceiveTime(IOThunderboltTimeStamp);
  virtual void setReceivedLength(unsigned long long);
  virtual void setStatus(int);
  virtual void complete(void);
  virtual void setFrameList(IOThunderboltFrameList *);
  virtual IOThunderboltFrameList* getFrameList();
  virtual void setMemoryDescriptor(IOMemoryDescriptor *);
  virtual IOMemoryDescriptor* getMemoryDescriptor();
  virtual void* getDMACommand();
  virtual void setOffset(unsigned long long);
  virtual unsigned long long getOffset();
  virtual void setLength(unsigned long long);
  virtual unsigned long long getLength();
  virtual void setCompletion(IOThunderboltReceiveCommand::Completion);
  virtual Completion getCompletion();
  virtual uint8_t getEOF();
  virtual uint8_t getSOF();
  virtual void getIsochReceiveTime(IOThunderboltTimeStamp *);
  virtual unsigned long long getReceivedLength();
  virtual void setRefcon(void *);
  virtual void* getRefcon();
  virtual void _RESERVEDIOThunderboltReceiveCommand0(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand1(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand2(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand3(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand4(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand5(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand6(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand7(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand8(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand9(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand10(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand11(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand12(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand13(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand14(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand15(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand16(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand17(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand18(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand19(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand20(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand21(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand22(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand23(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand24(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand25(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand26(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand27(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand28(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand29(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand30(void);
  virtual void _RESERVEDIOThunderboltReceiveCommand31(void);
};

