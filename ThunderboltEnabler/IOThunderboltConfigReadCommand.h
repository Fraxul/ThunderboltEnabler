#pragma once
#include "IOThunderboltConfigCommand.h"

class IOThunderboltConfigReadCommand  : public IOThunderboltConfigCommand {
  OSDeclareDefaultStructors(IOThunderboltConfigReadCommand);
public:

  static IOThunderboltConfigReadCommand* withController(IOThunderboltController*);
  virtual bool initWithController(IOThunderboltController *) override;

  virtual int prepareForExecution(IOThunderboltTransmitCommand *) override;
  virtual int processResponse(IOThunderboltReceiveCommand *) override;
  virtual void complete(int) override;

  virtual void setConfigSpace(unsigned int);
  virtual unsigned int getConfigSpace(void);
  virtual void setLength(unsigned int);
  virtual unsigned int getLength(void);
  virtual void setOffset(unsigned int);
  virtual unsigned int getOffset(void);
  virtual void setPort(unsigned int);
  virtual unsigned int getPort(void);
  virtual unsigned int getErrorCode(void);
  virtual void setResponseDataDescriptor(IOMemoryDescriptor *);
  virtual IOMemoryDescriptor* getResponseDataDescriptor(void);
  virtual unsigned int getResponsePort(void);

  virtual void _RESERVEDIOThunderboltConfigReadCommand0(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand1(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand2(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand3(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand4(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand5(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand6(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand7(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand8(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand9(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand10(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand11(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand12(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand13(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand14(void);
  virtual void _RESERVEDIOThunderboltConfigReadCommand15(void);
};
