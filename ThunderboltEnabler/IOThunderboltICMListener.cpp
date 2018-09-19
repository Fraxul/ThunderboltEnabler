#include "IOThunderboltICMListener.h"
#include "IOThunderboltConnectionManager.h"
#include "IOThunderboltController.h"
#include "IOThunderboltReceiveCommand.h"
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "tb_constants.h"

// Defined in IOThunderboltFamily
extern "C" {
  uint32_t IOThunderboltCRC32(void*, uint32_t);
};

OSDefineMetaClassAndStructors(IOThunderboltICMListener, IOThunderboltControlPathListener);

/*static*/  IOThunderboltICMListener* IOThunderboltICMListener::withController(IOThunderboltController* controller) {
  IOThunderboltICMListener* cmd = OSTypeAlloc(IOThunderboltICMListener);
  if (cmd) {
    if (!cmd->initWithController(controller)) {
      OSSafeReleaseNULL(cmd);
    }
  }
  return cmd;
}

bool IOThunderboltICMListener::initWithController(IOThunderboltController* controller_) {
  if (!IOThunderboltControlPathListener::initWithController(controller_)) {
    return false;
  }

  m_controller = controller_;

  return true;
}

void IOThunderboltICMListener::processResponse(IOThunderboltReceiveCommand* rxCommand) {
  kprintf("IOThunderboltICMListener::processResponse: rxCommand=%p\n", rxCommand);
  if (!rxCommand)
    return;

  size_t rxWords = rxCommand->getReceivedLength() / 4;
  kprintf("rxCommand sof=0x%x eof=0x%x\n", rxCommand->getSOF(), rxCommand->getEOF());
  uint32_t* rxMemRaw = reinterpret_cast<uint32_t*>(static_cast<IOBufferMemoryDescriptor*>(rxCommand->getMemoryDescriptor())->getBytesNoCopy());
  for (size_t i = 0; i < rxWords; ++i) {
    kprintf("   %03x  %08x\n", i * 4, rxMemRaw[i]);
  }

  uint32_t crc_wire = OSSwapBigToHostInt32(rxMemRaw[rxWords - 1]);
  uint32_t crc_computed = IOThunderboltCRC32(rxMemRaw, (rxWords - 1) * 4);
  if (crc_wire != crc_computed) {
    kprintf("IOThunderboltICMListener::processResponse(): bad CRC: wire is %08x, computed is %08x\n", crc_wire, crc_computed);
    // TODO CRC computation is not working right, so we don't bail here yet
  }

  // Byteswap into a local buffer. Maximum packet size is 256 bytes / 64 dwords.
  uint32_t rxMem[64];
  bzero(rxMem, 256);
  for (size_t i = 0; i < rxWords; ++i) {
    rxMem[i] = OSSwapBigToHostInt32(rxMemRaw[i]);
  }

  icm_pkg_header* header = reinterpret_cast<icm_pkg_header*>(rxMem);
  kprintf("icm_pkg_header code=%x flags=%x packet_id=%x total_packets=%x\n", header->code, header->flags, header->packet_id, header->total_packets);

  switch (header->code) {

    case ICM_EVENT_DEVICE_CONNECTED:
      handleDeviceConnected(reinterpret_cast<icm_fr_event_device_connected*>(rxMem));
      break;

    case ICM_EVENT_DEVICE_DISCONNECTED:
      handleDeviceDisconnected(reinterpret_cast<icm_fr_event_device_disconnected*>(rxMem));
      break;

    case ICM_EVENT_XDOMAIN_CONNECTED:
    case ICM_EVENT_XDOMAIN_DISCONNECTED:

    default:
      printf("IOThunderboltICMListener: Unhandled ICM event type %x\n", header->code);
      break;
  };
}


void IOThunderboltICMListener::handleDeviceConnected(icm_fr_event_device_connected* evt) {
  uuid_string_t uuidstr;
  uuid_unparse(evt->endpoint_uuid, uuidstr);
  kprintf("handleDeviceConnected(icm_fr): uuid=%s connection_key=%x connection_id=%x link_info=%x unk1=%x\n  endpoint_name=%s\n",
    uuidstr, evt->connection_key & 0xff, evt->connection_id & 0xff, evt->link_info & 0xffff, evt->unknown1 & 0xffff, evt->endpoint_name);

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleDeviceConnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }
}

void IOThunderboltICMListener::handleDeviceDisconnected(icm_fr_event_device_disconnected* evt) {
  kprintf("handleDeviceDisconnected(icm_fr): reserved=%x link_info=%x\n", evt->reserved & 0xffff, evt->link_info & 0xffff);

  IOThunderboltConnectionManager* cm = m_controller->getConnectionManager();
  if (cm) {
    cm->startRescan();
  } else {
    kprintf("handleDeviceDisconnected: controller doesn't (yet) have a CM to forward the plug event to\n");
  }
}

