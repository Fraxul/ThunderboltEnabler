#pragma once
#include <libkern/c++/OSObject.h>

class IOThunderboltReceiveCommand;
class IOThunderboltController;

class IOThunderboltControlPathListener : public OSObject {
  OSDeclareDefaultStructors(IOThunderboltControlPathListener);
public:
  typedef void* Completion;

  // OSObject overrides
  virtual void free() override;


  virtual void processResponse(IOThunderboltReceiveCommand*);
  virtual bool initWithController(IOThunderboltController*);
  virtual void setCompletion(IOThunderboltControlPathListener::Completion);
  virtual Completion getCompletion(void);
  virtual void callCompletion(IOThunderboltReceiveCommand*);

  virtual void _RESERVEDIOThunderboltControlPathListener0(void);
  virtual void _RESERVEDIOThunderboltControlPathListener1(void);
  virtual void _RESERVEDIOThunderboltControlPathListener2(void);
  virtual void _RESERVEDIOThunderboltControlPathListener3(void);
  virtual void _RESERVEDIOThunderboltControlPathListener4(void);
  virtual void _RESERVEDIOThunderboltControlPathListener5(void);
  virtual void _RESERVEDIOThunderboltControlPathListener6(void);
  virtual void _RESERVEDIOThunderboltControlPathListener7(void);
  virtual void _RESERVEDIOThunderboltControlPathListener8(void);
  virtual void _RESERVEDIOThunderboltControlPathListener9(void);
  virtual void _RESERVEDIOThunderboltControlPathListener10(void);
  virtual void _RESERVEDIOThunderboltControlPathListener11(void);
  virtual void _RESERVEDIOThunderboltControlPathListener12(void);
  virtual void _RESERVEDIOThunderboltControlPathListener13(void);
  virtual void _RESERVEDIOThunderboltControlPathListener14(void);
  virtual void _RESERVEDIOThunderboltControlPathListener15(void);
  virtual void _RESERVEDIOThunderboltControlPathListener16(void);
  virtual void _RESERVEDIOThunderboltControlPathListener17(void);
  virtual void _RESERVEDIOThunderboltControlPathListener18(void);
  virtual void _RESERVEDIOThunderboltControlPathListener19(void);
  virtual void _RESERVEDIOThunderboltControlPathListener20(void);
  virtual void _RESERVEDIOThunderboltControlPathListener21(void);
  virtual void _RESERVEDIOThunderboltControlPathListener22(void);
  virtual void _RESERVEDIOThunderboltControlPathListener23(void);
  virtual void _RESERVEDIOThunderboltControlPathListener24(void);
  virtual void _RESERVEDIOThunderboltControlPathListener25(void);
  virtual void _RESERVEDIOThunderboltControlPathListener26(void);
  virtual void _RESERVEDIOThunderboltControlPathListener27(void);
  virtual void _RESERVEDIOThunderboltControlPathListener28(void);
  virtual void _RESERVEDIOThunderboltControlPathListener29(void);
  virtual void _RESERVEDIOThunderboltControlPathListener30(void);
  virtual void _RESERVEDIOThunderboltControlPathListener31(void);

private:
  // Enough to push derived classes' members past the end of this class,
  // which is 0x38 bytes long.
  char pad[0x40];
};

