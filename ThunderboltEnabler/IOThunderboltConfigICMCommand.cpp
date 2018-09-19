#include "IOThunderboltConfigICMCommand.h"
#include "IOThunderboltReceiveCommand.h"
#include "IOThunderboltTransmitCommand.h"
#include <libkern/OSByteOrder.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "tb_constants.h"

// Defined in IOThunderboltFamily
extern "C" {
  uint32_t IOThunderboltCRC32(void*, uint32_t);
};

OSDefineMetaClassAndStructors(IOThunderboltConfigICMCommand, IOThunderboltConfigCommand);

/*static*/  IOThunderboltConfigICMCommand* IOThunderboltConfigICMCommand::withController(IOThunderboltController* controller) {
  IOThunderboltConfigICMCommand* cmd = OSTypeAlloc(IOThunderboltConfigICMCommand);
  if (cmd) {
    if (!cmd->initWithController(controller)) {
      OSSafeReleaseNULL(cmd);
    }
  }
  return cmd;
}

bool IOThunderboltConfigICMCommand::initWithController(IOThunderboltController* controller) {
  if (!IOThunderboltConfigCommand::initWithController(controller))
    return false;

  m_requestData = NULL;
  m_requestDataSize = 0;

  m_responseData = NULL;
  m_responseDataSize = 0;

  // Set some sensible defaults.
  this->setRouteString(0);

  return true;
}

void IOThunderboltConfigICMCommand::free() {
  if (m_requestData) {
    IOFree(m_requestData, m_requestDataSize);
    m_requestData = NULL;
    m_requestDataSize = 0;
  }
  if (m_responseData) {
    IOFree(m_responseData, m_responseDataSize);
    m_responseData = NULL;
    m_responseDataSize = 0;
  }

  IOThunderboltConfigCommand::free();
}

void IOThunderboltConfigICMCommand::setRequestData(const void* data, size_t size) {
  if (m_requestData) {
    IOFree(m_requestData, m_requestDataSize);
    m_requestData = NULL;
    m_requestDataSize = 0;
  }

  if (!size)
    return;

  if (size & 3) {
    panic("IOThunderboltConfigICMCommand::setRequestData: invalid data length (must be a multiple of 4)");
  }  

  m_requestData = reinterpret_cast<char*>(IOMalloc(size));
  m_requestDataSize = size;
  memcpy(m_requestData, data, size);
}

/*
void IOThunderboltConfigICMCommand::initWithController(IOThunderboltController* controller) {
  IOThunderboltConfigCommand::initWithController(controller);
}
*/

int IOThunderboltConfigICMCommand::prepareForExecution(IOThunderboltTransmitCommand* txCommand) {
  kprintf("IOThunderboltConfigICMCommand::prepareForExecution()\n");
  uint32_t* txMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(txCommand->getMemoryDescriptor())->getBytesNoCopy());

  /*** this command doesn't have a route string, so this is disabled
  txMem[0] = OSSwapHostToBigInt32(static_cast<uint32_t>((this->getRouteString() >> 32) & 0xffffffffULL));
  txMem[1] = OSSwapHostToBigInt32(static_cast<uint32_t>(this->getRouteString() & 0xffffffffULL));
  */

  size_t requestDWords = m_requestDataSize / 4;
  for (size_t i = 0; i < requestDWords; ++i) {
    txMem[i] = OSSwapHostToBigInt32(reinterpret_cast<uint32_t*>(m_requestData)[i]);
  }
  txMem[requestDWords] = OSSwapHostToBigInt32(IOThunderboltCRC32(txMem, requestDWords * 4));

  // add 4 bytes for the CRC32 dword
  txCommand->setLength(m_requestDataSize + 4);
  txCommand->setSOF(TB_CFG_PKG_ICM_CMD);
  txCommand->setEOF(TB_CFG_PKG_ICM_CMD);

  return 0;
}

int IOThunderboltConfigICMCommand::processRequestComplete(IOThunderboltTransmitCommand* txCommand) {
  kprintf("IOThunderboltConfigICMCommand::processRequestComplete()\n");
  return 0;
}

int IOThunderboltConfigICMCommand::processResponse(IOThunderboltReceiveCommand* rxCommand) {
  size_t responseLength = rxCommand->getReceivedLength();
  kprintf("IOThunderboltConfigICMCommand::processResponse(): response sof=0x%x eof=0x%x length = %zu\n", rxCommand->getSOF(), rxCommand->getEOF(), responseLength);

  if (rxCommand->getEOF() != TB_CFG_PKG_ICM_RESP) {
    return 0;
  }

  if (m_responseData) {
    IOFree(m_responseData, m_responseDataSize);
    m_responseData = NULL;
    m_responseDataSize = 0;
  }

  uint32_t* rxMem = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(rxCommand->getMemoryDescriptor())->getBytesNoCopy());
  if (!responseLength)
    return 3;

  size_t rxWords = responseLength / 4;

  // Verify CRC
  uint32_t crc_wire = OSSwapBigToHostInt32(rxMem[rxWords - 1]);
  uint32_t crc_computed = IOThunderboltCRC32(rxMem, (rxWords - 1) * 4);
  if (crc_wire != crc_computed) {
    kprintf("IOThunderboltConfigICMCommand::processResponse(): bad CRC: wire is %08x, computed is %08x\n", crc_wire, crc_computed);
    // TODO CRC computation is not working right, so we don't bail here yet
  }

  m_responseDataSize = responseLength - 4;
  m_responseData = static_cast<char*>(IOMalloc(m_responseDataSize));
  for (size_t i = 0; i < (rxWords - 1); ++i) {
    reinterpret_cast<uint32_t*>(m_responseData)[i] = OSSwapBigToHostInt32(rxMem[i]);
  }

  return 1;
}

