#pragma once
#include <libkern/c++/OSObject.h>
#include <IOKit/IOCommand.h>
class IOThunderboltController;

class IOThunderboltCommand : public IOCommand {
  OSDeclareDefaultStructors(IOThunderboltCommand);
public:
  virtual bool initWithController(IOThunderboltController *);

  virtual void _RESERVEDIOThunderboltCommand0(void);
  virtual void _RESERVEDIOThunderboltCommand1(void);
  virtual void _RESERVEDIOThunderboltCommand2(void);
  virtual void _RESERVEDIOThunderboltCommand3(void);
  virtual void _RESERVEDIOThunderboltCommand4(void);
  virtual void _RESERVEDIOThunderboltCommand5(void);
  virtual void _RESERVEDIOThunderboltCommand6(void);
  virtual void _RESERVEDIOThunderboltCommand7(void);
  virtual void _RESERVEDIOThunderboltCommand8(void);
  virtual void _RESERVEDIOThunderboltCommand9(void);
  virtual void _RESERVEDIOThunderboltCommand10(void);
  virtual void _RESERVEDIOThunderboltCommand11(void);
  virtual void _RESERVEDIOThunderboltCommand12(void);
  virtual void _RESERVEDIOThunderboltCommand13(void);
  virtual void _RESERVEDIOThunderboltCommand14(void);
  virtual void _RESERVEDIOThunderboltCommand15(void);

private:
  // Enough to push derived classes' members past the end of this class,
  // which is 0x98 bytes long in total.
  char pad[0xa0];
};


