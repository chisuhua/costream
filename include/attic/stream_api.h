#ifndef STREAM_API_H_
#define STREAM_API_H_



#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


namespace steam {

typedef status {
    SUCCESS,
    ERROR,
} status_t;

typedef struct signal_s {
  uint64_t handle;
} signal_t;

typedef void* stream_handle_t

#ifdef CO_LARGE_MODEL
  typedef int64_t signal_value_t;
#else
  typedef int32_t signal_value_t;
#endif


struct signal_create_args {
    signal_value_t initial_value;
    uint32_t num_consumers;
    const agent_t *consumers;
    signal_t *signal;
};

struct signal_destroy_args {
    signal_t signal;
};

struct signal_load_relaxed_args {
    signal_t signal;
};

struct signal_store_relaxed_args {
    signal_t signal;
};

struct signal_load_screlease_args {
    signal_t signal;
};

struct signal_store_screlease_args {
    signal_t signal;
};

enum class signal_condition_t {
    CONDITION_EQ = 0,
    CONDITION_NE = 1,
    CONDITION_LT = 2,
    CONDITION_GTE = 3
};

enum class wait_state_t {
    // The application thread may be rescheduled while waiting on the signal.
    BLOCKED = 0,
    // The application thread stays active while waiting on a signal.
    ACTIVE = 1
} ;

/* @brief Wait until a signal value satisfies a specified condition, or a certain amount of time has elapsed.
 * @details A wait operation can spuriously resume at any time sooner than the
 * timeout (for example, due to system or other external factors) even when the
 * condition has not been met.
 *
 * The function is guaranteed to return if the signal value satisfies the
 * condition at some point in time during the wait, but the value returned to
 * the application might not satisfy the condition. The application must ensure
 * that signals are used in such way that wait wakeup conditions are not
 * invalidated before dependent threads have woken up.
 *
 * When the wait operation internally loads the value of the passed signal, it
 * uses the memory order indicated in the function name.
 * @param[in] timeout_hint Maximum duration of the wait.  Specified in the same
 * unit as the system timestamp. The operation might block for a shorter or
 * longer time even if the condition is not met. A value of UINT64_MAX indicates
 * no maximum.
 *
 * @param[in] wait_state_hint Hint used by the application to indicate the
 * preferred waiting state. The actual waiting state is ultimately decided by
 * HSA runtime and may not match the provided hint. A value of
 * ::HSA_WAIT_STATE_ACTIVE may improve the latency of response to a signal
 * update by avoiding rescheduling overhead.
*/
#define SIGNAL_WAIT_ARGS                   \
    signal_t signal;                \
    signal_condition_t condition;   \
    signal_value_t compare_value;   \
    uint64_t timeout_hint;          \
    wait_state_t wait_state_hint;

struct signal_wait_scacquire_args {
    SIGNAL_WAIT_ARGS
};

struct signal_wait_relaxed_args {
    SIGNAL_WAIT_ARGS
};

typedef struct signal_group_s {
    uint64_t handle;
} signal_group_t;

/*
 * @param[in] num_signals Number of elements in @p signals. Must not be 0.
 * @param[in] signals List of signals in the group. The list must not contain
 * any repeated elements. Must not be NULL.
 * @param[in] num_consumers Number of elements in @p consumers. Must not be 0.
 */
struct signal_group_create_args {
    uint32_t num_signals;
    const signal_t *signals;
    uint32_t num_consumers;
    const agent_t *consumers;
    signal_group_t *signal_group;
};

struct signal_group_destroy_args {
    signal_group_t signal_group;
};

/*  @param[in] conditions List of conditions. Each condition, and the value at
 * the same index in @p compare_values, is used to compare the value of the
 * signal at that index in @p signal_group (the signal passed by the application
 * to ::hsa_signal_group_create at that particular index). The size of @p
 * conditions must not be smaller than the number of signals in @p signal_group;
 * any extra elements are ignored. Must not be NULL.
 *
 * @param[in] compare_values List of comparison values.  The size of @p
 * compare_values must not be smaller than the number of signals in @p
 * signal_group; any extra elements are ignored. Must not be NULL.
 *
 * @param[out] signal Signal in the group that satisfied the associated
 * condition. If several signals satisfied their condition, the function can
 * return any of those signals. Must not be NULL.
 *
 * @param[out] value Observed value for @p signal, which might no longer satisfy
 * the specified condition. Must not be NULL.
 */

#define SIGNAL_GROUP_WAIT_ARGS      \
    signal_group_t signal_group;    \
    signal_condition_t *condition;  \
    signal_value_t *compare_value;  \
    wait_state_t wait_state_hint;   \
    signal_t *signal;               \
    signal_value_t *value

struct signal_group_wait_any_scacquire_args {
    SIGNAL_WAIT_ARGS
};

struct signal_group_wait_any_relaxed_args {
    SIGNAL_WAIT_ARGS
};


typedef enum {
  // Queue supports multiple producers. Use of multiproducer queue mechanics is required.
  QUEUE_TYPE_MULTI = 0,
  // Queue only supports a single producer. In some scenarios, the application
  // may want to limit the submission of AQL packets to a single agent.
  QUEUE_TYPE_SINGLE = 1,
  /**
   * Queue supports multiple producers and cooperative dispatches. Cooperative
   * dispatches are able to use GWS synchronization. Queues of this type may be
   * limited in number. The runtime may return the same queue to serve multiple
   * ::hsa_queue_create calls when this type is given. Callers must inspect the
   * returned queue to discover queue size. Queues of this type are reference
   * counted and require a matching number of ::hsa_queue_destroy calls to
   * release. Use of multiproducer queue mechanics is required. See
   * ::HSA_AMD_AGENT_INFO_COOPERATIVE_QUEUES to query agent support for this
   * type.
   */
  QUEUE_TYPE_COOPERATIVE = 2
} queue_type_t;

// fixed-size type used to represent ::hsa_queue_type_t constants.
typedef uint32_t queue_type32_t;

typedef enum {
  // Queue supports kernel dispatch packets.
  QUEUE_FEATURE_KERNEL_DISPATCH = 1,
  // Queue supports agent dispatch packets.
  QUEUE_FEATURE_AGENT_DISPATCH = 2
} queue_feature_t;

typedef struct queue_s {
  uint64_t handle;
} queue_t;

struct queue_create_args {
    agent_t agent,
    uint32_t size,
    queue_type32_t type;
    void (*callback)(hsa_status_t status, hsa_queue_t *source, void *data);
    void *data;
    uint32_t private_segment_size;
    uint32_t group_segment_size;
    queue_t **queue;
};

struct queue_destroy_args {
    queue_t *queue;
};

struct queue_inactive_args {
    queue_t *queue;
};

#define QUEUE_LOAD_API(op, order)                \
struct queue_load_##op##_index_##order##_args {  \
    const queue_t *queue;                        \
};

#define QUEUE_STORE_API(op, order)               \
struct queue_store_##op##_index_##order##_args { \
    const queue_t *queue;                        \
    uint64_t value;                              \
};

#define QUEUE_OP_API(op, order)                 \
struct queue_##op##_write_index_##order##_args {    \
    const queue_t *queue;                        \
    uint64_t value;                              \
};


QUEUE_LOAD_API(read, scacqurie)
QUEUE_LOAD_API(read, relaxed)
QUEUE_LOAD_API(write, scacqurie)
QUEUE_LOAD_API(write, relaxed)

QUEUE_STORE_API(write, relaxed)
QUEUE_STORE_API(write, screlease)
QUEUE_STORE_API(read, relaxed)
QUEUE_STORE_API(read, screlease)

QUEUE_OP_API(cas, scacq_screl)
QUEUE_OP_API(cas, scacquire)
QUEUE_OP_API(cas, relaxed)
QUEUE_OP_API(cas, screlease)

QUEUE_OP_API(add, scacq_screl)
QUEUE_OP_API(add, scacquire)
QUEUE_OP_API(add, relaxed)
QUEUE_OP_API(add, screlease)

 // * @brief Width (in bits) of the sub-fields in ::packet_header_t.
typedef enum {
   PACKET_HEADER_WIDTH_TYPE = 8,
   PACKET_HEADER_WIDTH_BARRIER = 1,
   PACKET_HEADER_WIDTH_SCACQUIRE_FENCE_SCOPE = 2,
   /**
    * @deprecated Use HSA_PACKET_HEADER_WIDTH_SCACQUIRE_FENCE_SCOPE.
    */
   PACKET_HEADER_WIDTH_ACQUIRE_FENCE_SCOPE = 2,
   PACKET_HEADER_WIDTH_SCRELEASE_FENCE_SCOPE = 2,
   /**
    * @deprecated Use HSA_PACKET_HEADER_WIDTH_SCRELEASE_FENCE_SCOPE.
    */
   PACKET_HEADER_WIDTH_RELEASE_FENCE_SCOPE = 2
} packet_header_width_t;

/**
 * @brief Sub-fields of the kernel dispatch packet @a setup field. The offset
 * (with respect to the address of @a setup) of a sub-field is identical to its
 * enumeration constant. The width of each sub-field is determined by the
 * corresponding value in ::kernel_dispatch_packet_setup_width_t. The
 * offset and the width are expressed in bits.
 */
typedef enum {
   // Number of dimensions of the grid. Valid values are 1, 2, or 3.
   KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS = 0
} kernel_dispatch_packet_setup_t;

// * @brief Width (in bits) of the sub-fields in kernel_dispatch_packet_setup_t.
typedef enum {
   KERNEL_DISPATCH_PACKET_SETUP_WIDTH_DIMENSIONS = 2
} kernel_dispatch_packet_setup_width_t;

// TODO schi add for packet callback, it can merge with core::HsaEventCallback in next
typedef void (*EventCallback)(status_t status, queue_t* source, void* data);

/**
 * @brief AQL kernel dispatch packet
 */
typedef struct kernel_dispatch_packet_s {
  uint16_t header;
  uint16_t kernel_ctrl;
  uint16_t workgroup_size_x;
  uint16_t workgroup_size_y;
  uint16_t workgroup_size_z;
  uint32_t kernel_mode;
  uint16_t grid_size_x;
  uint16_t grid_size_y;

  uint16_t grid_size_z;
  uint32_t private_segment_size;
  uint32_t group_segment_size;
  uint64_t kernel_object;
  signal_t completion_signal;

#ifdef LARGE_MODEL
  void* kernarg_address;
#elif defined LITTLE_ENDIAN
  // The buffer must be allocated using ::hsa_memory_allocate,
  void* kernarg_address;
  uint32_t reserved1;
#else
  uint32_t reserved1;
  void* kernarg_address;
#endif
  EventCallback callback;
} kernel_dispatch_packet_t;

/**
 * @brief Agent dispatch packet.
 */
typedef struct agent_dispatch_packet_s {
  // used * packet type. The parameters are described by ::hsa_packet_header_t.
  uint16_t header;
  // Application-defined function to be performed by the destination agent.
  uint16_t type;
  uint32_t reserved0;
#ifdef LARGE_MODEL
  void* return_address;
#elif defined LITTLE_ENDIAN
  void* return_address;
  uint32_t reserved1;
#else
  uint32_t reserved1;
  void* return_address;
#endif
  // Function arguments.
  uint64_t arg[4];
  // uint64_t reserved2;
  EventCallback callback;
  signal_t completion_signal;

} agent_dispatch_packet_t;

typedef struct hsa_barrier_and_packet_s {
  uint16_t header;
  uint16_t reserved0;
  uint32_t reserved1;
  signal_t dep_signal[5];
  // uint64_t reserved2;
  EventCallback callback;
  // * special signal handle 0 to indicate that no signal is used.
  signal_t completion_signal;
} barrier_and_packet_t;

typedef struct barrier_or_packet_s {
  uint16_t header;
  uint16_t reserved0;
  uint32_t reserved1;

  signal_t dep_signal[5];
  // uint64_t reserved2;
  EventCallback callback;
  signal_t completion_signal;
} barrier_or_packet_t;

typedef struct dma_copy_packet_s {
  uint16_t header;
  uint16_t reserved0;
  //uint32_t reserved1;
  hsa_signal_t dep_signal[1];

  // dma_copy_dir copy_dir;
  const void* src;
  void* dst;
  uint64_t bytes; // DMA Copy Bytes

  signal_t completion_signal;
  uint32_t reserve[3];

} dma_copy_packet_t;


typedef struct task_packet_s {
  uint16_t header;
  uint16_t reserved0;
  signal_t dep_signal[1];
  const void* task;
  void* dst;
  uint64_t bytes;
  signal_t completion_signal;
  uint32_t reserve[3];
} task_packet_t;

struct AqlPacket {

  union {
    kernel_dispatch_packet_t dispatch;
    barrier_and_packet_t barrier_and;
    barrier_or_packet_t barrier_or;
    agent_dispatch_packet_t agent;
    dma_copy_packet_t dma_copy;
    task_packet_t task;
  };

  uint8_t type() const {
    return ((dispatch.header >> PACKET_HEADER_TYPE) &
                      ((1 << PACKET_HEADER_WIDTH_TYPE) - 1));
  }

  bool IsValid() const {
    return (type() <= PACKET_TYPE_MAX) & (type() != PACKET_TYPE_INVALID);
  }

  std::string string() const {
    std::stringstream string;
    uint8_t type = this->type();

    const char* type_names[] = {
        "PACKET_TYPE_VENDOR_SPECIFIC", "PACKET_TYPE_INVALID",
        "PACKET_TYPE_KERNEL_DISPATCH", "PACKET_TYPE_BARRIER_AND",
        "PACKET_TYPE_AGENT_DISPATCH",  "PACKET_TYPE_BARRIER_OR"};

    string << "type: " << type_names[type]
           << "\nbarrier: " << ((dispatch.header >> PACKET_HEADER_BARRIER) &
                                ((1 << PACKET_HEADER_WIDTH_BARRIER) - 1))
           << "\nacquire: " << ((dispatch.header >> PACKET_HEADER_SCACQUIRE_FENCE_SCOPE) &
                                ((1 << PACKET_HEADER_WIDTH_SCACQUIRE_FENCE_SCOPE) - 1))
           << "\nrelease: " << ((dispatch.header >> PACKET_HEADER_SCRELEASE_FENCE_SCOPE) &
                                ((1 << PACKET_HEADER_WIDTH_SCRELEASE_FENCE_SCOPE) - 1));

    if (type == PACKET_TYPE_KERNEL_DISPATCH) {
      string << "\nkernel_ctrl: " << dispatch.kernel_ctrl
             << "\nworkgroup_size: " << dispatch.workgroup_size_x << ", "
             << dispatch.workgroup_size_y << ", " << dispatch.workgroup_size_z
             << "\ngrid_size: " << dispatch.grid_size_x << ", "
             << dispatch.grid_size_y << ", " << dispatch.grid_size_z
             << "\nprivate_size: " << dispatch.private_segment_size
             << "\ngroup_size: " << dispatch.group_segment_size
             << "\nkernel_object: " << dispatch.kernel_object
             << "\nkern_arg: " << dispatch.kernarg_address
             << "\nsignal: " << dispatch.completion_signal.handle;
    }

    if ((type == PACKET_TYPE_BARRIER_AND) ||
        (type == PACKET_TYPE_BARRIER_OR)) {
      for (int i = 0; i < 5; i++)
        string << "\ndep[" << i << "]: " << barrier_and.dep_signal[i].handle;
      string << "\nsignal: " << barrier_and.completion_signal.handle;
    }

    return string.str();
  }
};

}

#ifdef __cplusplus
}  // end extern "C" block
#endif

#endif  // header guard
