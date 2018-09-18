#pragma once
#include "IOThunderboltCommand.h"
class IOMemoryDescriptor;

class IOThunderboltTransmitCommand : public IOThunderboltCommand {
  OSDeclareDefaultStructors(IOThunderboltTransmitCommand);
public:
  typedef void* Completion;

  virtual bool initWithController(IOThunderboltController *) override;

  virtual void setInProgress(bool);
  virtual bool isInProgress();
  virtual void setStatus(int);
  virtual void complete();
  virtual void setFrameList(IOThunderboltFrameList *);
  virtual IOThunderboltFrameList* getFrameList();
  virtual void setMemoryDescriptor(IOMemoryDescriptor *);
  virtual IOMemoryDescriptor* getMemoryDescriptor();
  virtual void* getDMACommand();
  virtual void setOffset(unsigned long long);
  virtual unsigned long long  getOffset();
  virtual void setLength(unsigned long long);
  virtual unsigned long long getLength();
  virtual void setCompletion(IOThunderboltTransmitCommand::Completion);
  virtual void getCompletion();
  virtual void setEOF(uint8_t);
  virtual uint8_t getEOF();
  virtual void setSOF(uint8_t);
  virtual uint8_t getSOF();
  virtual void setIsochMode(bool);
  virtual bool getIsochMode();
  virtual void setIsochTransmitTime(IOThunderboltTimeStamp);
  virtual void getIsochTransmitTime(IOThunderboltTimeStamp *);
  virtual void setRefcon(void *);
  virtual void* getRefcon();
  virtual void _RESERVEDIOThunderboltTransmitCommand0(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand1(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand2(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand3(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand4(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand5(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand6(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand7(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand8(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand9(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand10(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand11(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand12(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand13(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand14(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand15(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand16(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand17(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand18(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand19(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand20(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand21(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand22(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand23(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand24(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand25(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand26(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand27(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand28(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand29(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand30(void);
  virtual void _RESERVEDIOThunderboltTransmitCommand31(void);
};

