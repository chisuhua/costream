#pragma once
#include <stdint.h>
#include "util/utils.h"

/*typedef struct device_s {
  uint64_t handle;
} device_t;
*/

#define QUEUE_ALIGN_BYTES (64)
#define QUEUE_ALIGN __ALIGNED__(QUEUE_ALIGN_BYTES)

#include "stream_api.h"

/*
namespace core {
class EventPool;
class StreamPool;
class SharedSignalPool_t;
class Device;
}
*/

typedef uint32_t queue_properties32_t;

// @brief Signal attribute flags.
typedef enum {
   //* Signal will only be consumed by AMD GPUs.  Limits signal consumption to
   //* AMD GPU devices only.  Ignored if @p num_consumers is not zero (all devices).
  SIGNAL_GPU_ONLY = 1,
  /**
   * Signal may be used for interprocess communication.
   * IPC signals can be read, written, and waited on from any process.
   * Profiling using an IPC enabled signal is only supported in a single process
   * at a time.  Producing profiling data in one process and consuming it in
   * another process is undefined.
   */
  SIGNAL_IPC = 2,
} signal_attribute_t;

typedef int64_t signal_kind64_t;
enum signal_kind_t {
  SIGNAL_KIND_INVALID = 0,
  SIGNAL_KIND_USER = 1,
  SIGNAL_KIND_DOORBELL = -1,
  SIGNAL_KIND_LEGACY_DOORBELL = -2
};

typedef uint64_t     QUEUEID;

typedef struct QueueResource_s
{
    QUEUEID     QueueId;    /** queue ID */
    /** Doorbell address to notify HW of a new dispatch */
    union {
        uint32_t*  Queue_DoorBell;
        uint64_t*  Queue_DoorBell_aql;
        uint64_t   QueueDoorBell;
    };

    /** virtual address to notify HW of queue write ptr value */
    union {
        uint32_t*  Queue_write_ptr;
        uint64_t*  Queue_write_ptr_aql;
        uint64_t   QueueWptrValue;
    };

    /** virtual address updated by HW to indicate current read location */
    union {
        uint32_t*  Queue_read_ptr;
        uint64_t*  Queue_read_ptr_aql;
        uint64_t   QueueRptrValue;
    };

} QueueResource;


// fixed-size type used to represent ::hsa_queue_type_t constants.
typedef uint32_t queue_type32_t;

typedef struct QUEUE_ALIGN co_queue_s {
  queue_type32_t type;
  // Queue features mask. This is a bit-field of ::hsa_queue_feature_t values.
  uint32_t features;

#ifdef LARGE_MODEL
  void* base_address;
#elif defined LITTLE_ENDIAN
  void* base_address; // Must be aligned to the size of an AQL packet.
  uint32_t reserved0;
#else
  uint32_t reserved0;
  void* base_address;
#endif
  /**
   * Signal object used by the application to indicate the ID of a packet that
   * is ready to be processed. The runtime manages the doorbell signal. If
   * the application tries to replace or destroy this signal, the behavior is
   * undefined.
   *
   * If @a type is ::HSA_QUEUE_TYPE_SINGLE, the doorbell signal value must be
   * updated in a monotonically increasing fashion. If @a type is
   * ::HSA_QUEUE_TYPE_MULTI, the doorbell signal value can be updated with any
   * value.
   */
  signal_t doorbell_signal;
  uint32_t size; // Maximum number of packets the queue can hold. Must be a power of 2.
  uint32_t reserved1;
  uint64_t id;      // Queue identifier, which is unique over the lifetime of the application.
  // queue_t queue_handle;
  volatile uint64_t write_dispatch_id;
  volatile uint64_t read_dispatch_id;
  volatile uint64_t max_legacy_doorbell_dispatch_id_plus_1;
  volatile uint32_t legacy_doorbell_lock;
  uint32_t read_dispatch_id_field_base_byte_offset;
  queue_properties32_t queue_properties;
  QueueResource queue_rsrc;  // schi add it
  signal_t queue_inactive_signal;
} co_queue_t;

#define SIGNAL_ALIGN_BYTES 64
#define SIGNAL_ALIGN __ALIGNED__(SIGNAL_ALIGN_BYTES)
typedef struct SIGNAL_ALIGN co_signal_s {
  signal_kind64_t kind;
  union {
    volatile int64_t value;
    volatile uint32_t* legacy_hardware_doorbell_ptr;
    volatile uint64_t* hardware_doorbell_ptr;
  };
  uint64_t event_mailbox_ptr;
  uint32_t event_id;
  uint32_t reserved1;
  uint64_t start_ts;
  uint64_t end_ts;
  union {
    co_queue_t* queue_ptr;
    uint64_t reserved2;
  };
  uint32_t reserved3[2];
} co_signal_t;

