#include <IOKit/IOService.h>

#define STUB_CLASS(classname) \
class classname : public IOService { \
  OSDeclareDefaultStructors(classname); \
public: \
  virtual bool start(IOService* provider) override { \
    IOService::start(provider); \
    return true; \
  } \
}; \
OSDefineMetaClassAndStructors(classname, IOService); \


STUB_CLASS(AppleThunderboltDPInAdapterStub);
STUB_CLASS(AppleThunderboltDPOutAdapterStub);
STUB_CLASS(AppleThunderboltPCIDownAdapterStub);
STUB_CLASS(AppleThunderboltPCIUpAdapterStub);

