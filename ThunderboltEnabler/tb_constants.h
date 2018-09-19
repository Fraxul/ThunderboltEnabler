#pragma once
#include <uuid/uuid.h>

#define BITS_PER_LONG 64
#define BIT(nr)     (1UL << (nr))
#define GENMASK(h, l) \
  (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define TB_CFG_HOPS     0
#define TB_CFG_PORT     1
#define TB_CFG_SWITCH   2
#define TB_CFG_COUNTERS 3

#define TB_CFG_PKG_ICM_CMD   11
#define TB_CFG_PKG_ICM_RESP  12

// icm package codes
#define ICM_GET_TOPOLOGY        0x1
#define ICM_DRIVER_READY        0x3
#define ICM_APPROVE_DEVICE      0x4
#define ICM_CHALLENGE_DEVICE    0x5
#define ICM_ADD_DEVICE_KEY      0x6
#define ICM_GET_ROUTE           0xa
#define ICM_APPROVE_XDOMAIN     0x10
#define ICM_DISCONNECT_XDOMAIN  0x11
#define ICM_PREBOOT_ACL         0x18

// icm package header flags
#define ICM_FLAGS_ERROR         BIT(0)
#define ICM_FLAGS_NO_KEY        BIT(1)
#define ICM_FLAGS_SLEVEL_SHIFT  3
#define ICM_FLAGS_SLEVEL_MASK   GENMASK(4, 3)
#define ICM_FLAGS_WRITE         BIT(7)

struct icm_pkg_header {
  icm_pkg_header() : code(0), flags(0), packet_id(0), total_packets(0) {}

  uint8_t code;
  uint8_t flags;
  uint8_t packet_id;
  uint8_t total_packets;
};

// icm event codes
#define ICM_EVENT_DEVICE_CONNECTED      3
#define ICM_EVENT_DEVICE_DISCONNECTED   4
#define ICM_EVENT_XDOMAIN_CONNECTED     6
#define ICM_EVENT_XDOMAIN_DISCONNECTED  7

struct icm_fr_event_device_connected {
  struct icm_pkg_header hdr;
  uuid_t endpoint_uuid;
  uint8_t connection_key;
  uint8_t connection_id;
  uint16_t link_info;

  uint16_t unknown1;
  char endpoint_name[214];
};

struct icm_fr_event_device_disconnected {
  struct icm_pkg_header hdr;
  uint16_t reserved;
  uint16_t link_info;
};

