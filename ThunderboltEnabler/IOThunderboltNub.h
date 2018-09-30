#pragma once
#include <IOKit/IOService.h>
class IOThunderboltController;

class IOThunderboltNub : public IOService {
  OSDeclareDefaultStructors(IOThunderboltNub);
public:
  // OSObject
  virtual void free() override;

  // IOService
  virtual IOReturn message(unsigned int,IOService *,void *) override;

  // IOThunderboltNub
  virtual bool initWithController(IOThunderboltController *);
  virtual IOThunderboltController* getController();
  virtual unsigned int getThunderboltDepth();
  virtual uint64_t getRouteString();

  virtual void _RESERVEDIOThunderboltNub0();
  virtual void _RESERVEDIOThunderboltNub1();
  virtual void _RESERVEDIOThunderboltNub2();
  virtual void _RESERVEDIOThunderboltNub3();
  virtual void _RESERVEDIOThunderboltNub4();
  virtual void _RESERVEDIOThunderboltNub5();
  virtual void _RESERVEDIOThunderboltNub6();
  virtual void _RESERVEDIOThunderboltNub7();
  virtual void _RESERVEDIOThunderboltNub8();
  virtual void _RESERVEDIOThunderboltNub9();
  virtual void _RESERVEDIOThunderboltNub10();
  virtual void _RESERVEDIOThunderboltNub11();
  virtual void _RESERVEDIOThunderboltNub12();
  virtual void _RESERVEDIOThunderboltNub13();
  virtual void _RESERVEDIOThunderboltNub14();
  virtual void _RESERVEDIOThunderboltNub15();
  virtual void _RESERVEDIOThunderboltNub16();
  virtual void _RESERVEDIOThunderboltNub17();
  virtual void _RESERVEDIOThunderboltNub18();
  virtual void _RESERVEDIOThunderboltNub19();
  virtual void _RESERVEDIOThunderboltNub20();
  virtual void _RESERVEDIOThunderboltNub21();
  virtual void _RESERVEDIOThunderboltNub22();
  virtual void _RESERVEDIOThunderboltNub23();
  virtual void _RESERVEDIOThunderboltNub24();
  virtual void _RESERVEDIOThunderboltNub25();
  virtual void _RESERVEDIOThunderboltNub26();
  virtual void _RESERVEDIOThunderboltNub27();
  virtual void _RESERVEDIOThunderboltNub28();
  virtual void _RESERVEDIOThunderboltNub29();
  virtual void _RESERVEDIOThunderboltNub30();
  virtual void _RESERVEDIOThunderboltNub31();
};

