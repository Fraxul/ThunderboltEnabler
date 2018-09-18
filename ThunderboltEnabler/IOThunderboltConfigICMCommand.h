#pragma once
#include "IOThunderboltConfigCommand.h"

class IOThunderboltConfigICMCommand : public IOThunderboltConfigCommand {
  OSDeclareDefaultStructors(IOThunderboltConfigICMCommand);
public:

  static IOThunderboltConfigICMCommand* withController(IOThunderboltController*);

  virtual bool initWithController(IOThunderboltController *) override;
  virtual void free() override;

  //virtual void initWithController(IOThunderboltController *) override;
  virtual int prepareForExecution(IOThunderboltTransmitCommand *) override;
  virtual int processRequestComplete(IOThunderboltTransmitCommand *) override;
  virtual int processResponse(IOThunderboltReceiveCommand *) override;


  void setRequestData(const void*, size_t);

  char* responseData() { return m_responseData; }
  size_t responseDataSize() { return m_responseDataSize; }

protected:
  char* m_requestData;
  size_t m_requestDataSize;

  char* m_responseData;
  size_t m_responseDataSize;
};

