#pragma once

typedef uint64_t IOPCIScalar;
struct IOPCIConfigEntry;

enum {
    kIOPCIRangeFlagMaximizeSize  = 0x00000001,
    kIOPCIRangeFlagNoCollapse    = 0x00000002,
    kIOPCIRangeFlagMaximizeRoot  = 0x00000004,
    kIOPCIRangeFlagSplay         = 0x00000008,
    kIOPCIRangeFlagRelocatable   = 0x00000010,
    kIOPCIRangeFlagReserve       = 0x00000020,
    kIOPCIRangeFlagPermanent     = 0x00000040,
    kIOPCIRangeFlagBar64         = 0x00000080,
};

enum {
    kIOPCIRangeBAR0               = 0,
    kIOPCIRangeBAR1               = 1,
    kIOPCIRangeBAR2               = 2,
    kIOPCIRangeBAR3               = 3,
    kIOPCIRangeBAR4               = 4,
    kIOPCIRangeBAR5               = 5,
    kIOPCIRangeExpansionROM       = 6,

    // order matches kIOPCIResourceType*
    kIOPCIRangeBridgeMemory       = 7,
    kIOPCIRangeBridgePFMemory     = 8,
    kIOPCIRangeBridgeIO           = 9,
    kIOPCIRangeBridgeBusNumber    = 10,

    kIOPCIRangeCount,

  kIOPCIRangeAllMask          = (1 << kIOPCIRangeCount) - 1,
  kIOPCIRangeAllBarsMask      = (1 << (kIOPCIRangeExpansionROM + 1)) - 1,
  kIOPCIRangeAllBridgeMask    =  (1 << kIOPCIRangeBridgeMemory)
                   | (1 << kIOPCIRangeBridgePFMemory)
                   | (1 << kIOPCIRangeBridgeIO)
                   | (1 << kIOPCIRangeBridgeBusNumber),


};

struct IOPCIRange
{
    IOPCIScalar         start;
    IOPCIScalar         size;
    IOPCIScalar         totalSize;
    IOPCIScalar         extendSize;
    IOPCIScalar         proposedSize;

    // end marker
    IOPCIScalar         end;
    IOPCIScalar         zero;

    IOPCIScalar         alignment;
    IOPCIScalar         minAddress;
    IOPCIScalar         maxAddress;

    uint8_t             type;
    uint8_t             resvB[3];
    uint32_t            flags;
    struct IOPCIRange * next;
    struct IOPCIRange * nextSubRange;
    struct IOPCIRange * allocations;

    struct IOPCIRange *  nextToAllocate;
    struct IOPCIConfigEntry * device;       // debug
};

// value of supportsHotPlug
enum
{
    kPCIHPTypeMask              = 0xf0,

    kPCIHPRoot                  = 0x01,
    kPCIHPRootParent            = 0x02,

    kPCIStatic                  = 0x00,
    kPCIStaticTunnel            = kPCIStatic | 0x01,
    kPCIStaticShared            = kPCIStatic | 0x02,

    kPCILinkChange              = 0x10,

    kPCIHotPlug                 = 0x20,
    kPCIHotPlugRoot             = kPCIHotPlug | kPCIHPRoot,

    kPCIHotPlugTunnel           = 0x30,
    kPCIHotPlugTunnelRoot       = kPCIHotPlugTunnel | kPCIHPRoot,
    kPCIHotPlugTunnelRootParent = kPCIHotPlugTunnel | kPCIHPRootParent,
};

struct IOPCIConfigEntry
{
    IOPCIConfigEntry *  parent;
    IOPCIConfigEntry *  child;
    IOPCIConfigEntry *  peer;
    uint32_t      id;
    uint32_t            classCode;
    IOPCIAddressSpace   space;
    uint32_t            vendorProduct;

  uint32_t      expressCapBlock;
  uint32_t      expressDeviceCaps1;

    IOPCIRange *        ranges[kIOPCIRangeCount];
    IOPCIRange          busResv;
    uint32_t            rangeBaseChanges;
    uint32_t            rangeSizeChanges;
    uint32_t            rangeRequestChanges;
    uint32_t            haveAllocs;

    uint32_t            deviceState;
    uint8_t             iterator;

    uint8_t             headerType;
    uint8_t             isBridge;
    uint8_t             countMaximize;
    uint8_t             isHostBridge;
    uint8_t             supportsHotPlug;
    uint8_t       linkInterrupts;
    uint8_t             clean64;
    uint8_t             secBusNum;  // bridge only
    uint8_t             subBusNum;  // bridge only

  uint32_t      linkCaps;
  uint16_t      expressCaps;
    uint8_t         expressMaxPayload;
    uint8_t         expressPayloadSetting;
//  uint16_t            pausedCommand;

    IORegistryEntry *   dtEntry;
    IORegistryEntry *   acpiDevice;
    IORegistryEntry *   dtNub;

  uint8_t *     configShadow;
};

