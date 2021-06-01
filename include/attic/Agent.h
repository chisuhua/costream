#pragma once
#include "Stream.h"
#include "StreamType.h"

typedef union
{
    uint32_t Value;
    struct
    {
        unsigned int HotPluggable        : 1;    // the node may be removed by some system action
                                                 // (event will be sent)
        unsigned int HSAMMUPresent       : 1;    // This node has an ATS/PRI 1.1 compatible
                                                 // translation agent in the system (e.g. IOMMUv2)
        unsigned int SharedWithGraphics  : 1;    // this HSA nodes' GPU function is also used for OS primary
                                                 // graphics render (= UI)
        unsigned int QueueSizePowerOfTwo : 1;    // This node GPU requires the queue size to be a power of 2 value
        unsigned int QueueSize32bit      : 1;    // This node GPU requires the queue size to be less than 4GB
        unsigned int QueueIdleEvent      : 1;    // This node GPU supports notification on Queue Idle
        unsigned int VALimit             : 1;    // This node GPU has limited VA range for platform
                                                 // (typical 40bit). Affects shared VM use for 64bit apps
        unsigned int WatchPointsSupported: 1;	 // Indicates if Watchpoints are available on the node.
        unsigned int WatchPointsTotalBits: 4;    // ld(Watchpoints) available. To determine the number use 2^value

        unsigned int DoorbellType        : 2;    // 0: This node has pre-1.0 doorbell characteristic
                                                 // 1: This node has 1.0 doorbell characteristic
                                                 // 2,3: reserved for future use
        unsigned int AQLQueueDoubleMap    : 1;	 // The unit needs a VA “double map”
        unsigned int DebugTrapSupported  : 1;    // Indicates if Debug Trap is supported on the node.
        unsigned int WaveLaunchTrapOverrideSupported: 1; // Indicates if Wave Launch Trap Override is supported on the node.
        unsigned int WaveLaunchModeSupported: 1; // Indicates if Wave Launch Mode is supported on the node.
        unsigned int PreciseMemoryOperationsSupported: 1; // Indicates if Precise Memory Operations are supported on the node.
        unsigned int SRAM_EDCSupport: 1;         // Indicates if GFX internal SRAM EDC/ECC functionality is active
        unsigned int Mem_EDCSupport: 1;          // Indicates if GFX internal DRAM/HBM EDC/ECC functionality is active
        unsigned int RASEventNotify: 1;          // Indicates if GFX extended RASFeatures and RAS EventNotify status is available
        unsigned int ASICRevision: 4;            // Indicates the ASIC revision of the chip on this node.
        unsigned int Reserved            : 6;
    } ui32;
} HSA_CAPABILITY;


typedef struct _HsaNodeProperties
{
    uint32_t       NumCPUCores;       // # of latency (= CPU) cores present on this HSA node.
                                       // This value is 0 for a HSA node with no such cores,
                                       // e.g a "discrete HSA GPU"
    uint32_t       NumFComputeCores;  // # of HSA throughtput (= GPU) FCompute cores ("SIMD") present in a node.
                                       // This value is 0 if no FCompute cores are present (e.g. pure "CPU node").
    uint32_t       NumMemoryBanks;    // # of discoverable memory bank affinity properties on this "H-NUMA" node.
    uint32_t       NumCaches;         // # of discoverable cache affinity properties on this "H-NUMA"  node.

    uint32_t       NumIOLinks;        // # of discoverable IO link affinity properties of this node
                                       // connecting to other nodes.

    uint32_t       CComputeIdLo;      // low value of the logical processor ID of the latency (= CPU)
                                       // cores available on this node
    uint32_t       FComputeIdLo;      // low value of the logical processor ID of the throughput (= GPU)
                                       // units available on this node

    HSA_CAPABILITY  Capability;        // see above

    uint32_t       MaxWavesPerSIMD;   // This identifies the max. number of launched waves per SIMD.
                                       // If NumFComputeCores is 0, this value is ignored.
    uint32_t       LDSSizeInKB;       // Size of Local Data Store in Kilobytes per SIMD Wavefront
    uint32_t       GDSSizeInKB;       // Size of Global Data Store in Kilobytes shared across SIMD Wavefronts

    uint32_t       WaveFrontSize;     // Number of SIMD cores per wavefront executed, typically 64,
                                       // may be 32 or a different value for some HSA based architectures

    uint32_t       NumShaderBanks;    // Number of Shader Banks or Shader Engines, typical values are 1 or 2


    uint32_t       NumArrays;         // Number of SIMD arrays per engine
    uint32_t       NumCUPerArray;     // Number of Compute Units (CU) per SIMD array
    uint32_t       NumSIMDPerCU;      // Number of SIMD representing a Compute Unit (CU)

    uint32_t       MaxSlotsScratchCU; // Number of temp. memory ("scratch") wave slots available to access,
                                       // may be 0 if HW has no restrictions

    // HSA_ENGINE_ID   EngineId;          // Identifier (rev) of the GPU uEngine or Firmware, may be 0

    uint16_t       VendorId;          // GPU vendor id; 0 on latency (= CPU)-only nodes
    uint16_t       DeviceId;          // GPU device id; 0 on latency (= CPU)-only nodes

    uint32_t       LocationId;        // GPU BDF (Bus/Device/function number) - identifies the device
                                       // location in the overall system
    uint64_t       LocalMemSize;       // Local memory size
    uint32_t       MaxEngineClockMhzFCompute;  // maximum engine clocks for CPU and
    uint32_t       MaxEngineClockMhzCCompute;  // GPU function, including any boost caopabilities,
    int32_t        DrmRenderMinor;             // DRM render device minor device number
    //uint16_t       MarketingName[HSA_PUBLIC_NAME_SIZE];   // Public name of the "device" on the node (board or APU name).
                                       // Unicode string
    //uint8_t        AMDName[HSA_PUBLIC_NAME_SIZE];   //CAL Name of the "device", ASCII
    // HSA_ENGINE_VERSION uCodeEngineVersions;
    // HSA_DEBUG_PROPERTIES DebugProperties; // Debug properties of this node.
    uint64_t       HiveID;            // XGMI Hive the GPU node belongs to in the system. It is an opaque and static
                                       // number hash created by the PSP
    uint32_t       NumSdmaEngines;    // number of PCIe optimized SDMA engines
    uint32_t       NumSdmaXgmiEngines;// number of XGMI optimized SDMA engines

    uint8_t        NumSdmaQueuesPerEngine;// number of SDMA queue per one engine
    uint8_t        NumCpQueues; // number of Compute queues
    uint8_t        NumGws;            // number of GWS barriers
    uint8_t        Reserved2;

    uint32_t       Domain;            // PCI domain of the GPU
    uint64_t       UniqueID;          // Globally unique immutable id
    uint8_t        Reserved[20];
} HsaNodeProperties;

#define HSA_ARGUMENT_ALIGN_BYTES 16
#define HSA_QUEUE_ALIGN_BYTES 64
#define HSA_PACKET_ALIGN_BYTES 64

class Agent {
    public:
    void DestroyQueue(QUEUEID QueueId);
    uint64_t QueueAllocator(uint32_t, uint32_t);
    void QueueDeallocator(AqlPacket*);
    HsaNodeProperties properties();

    void CreateQueue( HSA_QUEUE_TYPE Type,
					  uint32_t QueuePercentage,
					  HSA_QUEUE_PRIORITY Priority,
					  void *QueueAddress,
					  uint64_t QueueSizeInBytes,
					  HsaEvent *Event,
					  QueueResource *QueueResource);
};


