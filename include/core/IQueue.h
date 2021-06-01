#pragma once
#include <sstream>

#include "Shared.h"
#include "core/ISignal.h"
#include "util/atomic_helpers.h"
// #include "hsakmttypes.h"

// schi #include "core/inc/checked.h"
#include "device_type.h"

#include "util/utils.h"
// #include "Stream.h"

// #include "pps_queue.h"
enum amd_queue_properties_t {
    BITS_CREATE_ENUM_ENTRIES(QUEUE_PROPERTIES_ENABLE_TRAP_HANDLER, 0, 1),
    BITS_CREATE_ENUM_ENTRIES(QUEUE_PROPERTIES_IS_PTR64, 1, 1),
    BITS_CREATE_ENUM_ENTRIES(QUEUE_PROPERTIES_ENABLE_TRAP_HANDLER_DEBUG_SGPRS, 2, 1),
    BITS_CREATE_ENUM_ENTRIES(QUEUE_PROPERTIES_ENABLE_PROFILING, 3, 1),
    BITS_CREATE_ENUM_ENTRIES(QUEUE_PROPERTIES_RESERVED1, 4, 28)
};

namespace core {

// co_queue_t is derived from hsa_queue_t
class IQueue;
class StreamPool;

// typedef void (*EventCallback)(status_t status, IQueue* source, void* data);

/// @brief Helper structure to simplify conversion of co_queue_t and
/// core::Queue object.
struct SharedQueue {
    co_queue_t co_queue_;
    IQueue* core_queue_;

    SharedQueue()
    {
        memset(&co_queue_, 0, sizeof(co_queue_));
        core_queue_ = nullptr;
    }

    static __forceinline SharedQueue* Object(queue_t queue)
    {
        SharedQueue* ret = reinterpret_cast<SharedQueue*>(static_cast<uintptr_t>(queue.handle) - offsetof(SharedQueue, co_queue_));
        return ret;
    }

    static __forceinline queue_t Handle(const SharedQueue* shared_queue)
    {
        assert(shared_queue != nullptr && "Conversion on null Signal object.");
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&shared_queue->co_queue_));
        const queue_t queue_handle = { handle };
        return queue_handle;
    }
};

class SharedQueuePool : private BaseShared {
public:
    SharedQueuePool()
        : block_size_(minblock_)
    {
    }
    ~SharedQueuePool() { clear(); }

    SharedQueue* alloc();
    void free(SharedQueue* ptr);
    void clear();

private:
    static const size_t minblock_ = 4096 / sizeof(SharedQueue);
    KernelMutex lock_;
    std::vector<SharedQueue*> free_list_;
    std::vector<std::pair<void*, size_t>> block_list_;
    size_t block_size_;
};

template <typename Allocator = PageAllocator<SharedQueue>>
class LocalQueue {
public:
    SharedQueue* GetShared() const { return local_.shared_object(); }
    SharedQueue* GetSharedQueue() const { return local_.shared_object(); }

private:
    Shared<SharedQueue, Allocator> local_;
};

/*
Queue is intended to be an pure interface class and may be wrapped or replaced
by tools.  All funtions other than Convert and public_handle must be virtual.
*/
class IQueue : private LocalQueue<> {
public:
    IQueue(uint32_t queue_size_pkts)
        : LocalQueue<>()
        , queue_size_pkts_(queue_size_pkts)
        , co_queue_(GetShared()->co_queue_)
    {
        GetShared()->core_queue_ = this;
        // public_handle_ = Handle(this);
    }

    virtual ~IQueue() { }
    virtual void Destroy() { delete this; }

    static queue_t Handle(IQueue* core_queue)
    {
        assert(core_queue != nullptr && "Conversion on null Queue object.");
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&core_queue->co_queue_));
        const queue_t queue_handle = { handle };
        return queue_handle;
    }

    static IQueue* Object(const queue_t queue)
    {
        return (queue.handle != 0)
            ? reinterpret_cast<SharedQueue*>(reinterpret_cast<uintptr_t>(queue.handle) - offsetof(SharedQueue, co_queue_))->core_queue_
            : nullptr;
    }

    /// @brief Inactivate the queue object. Once inactivate a
    /// queue cannot be used anymore and must be destroyed
    virtual status_t Inactivate() = 0;

    virtual status_t SetPriority(HSA_QUEUE_PRIORITY priority) = 0;

    virtual uint64_t LoadReadIndexAcquire() = 0;

    virtual uint64_t LoadReadIndexRelaxed() = 0;

    virtual uint64_t LoadWriteIndexAcquire() = 0;

    virtual uint64_t LoadWriteIndexRelaxed() = 0;

    virtual void StoreReadIndexRelaxed(uint64_t value) = 0;
    virtual void StoreReadIndexRelease(uint64_t value) = 0;
    virtual void StoreWriteIndexRelaxed(uint64_t value) = 0;
    virtual void StoreWriteIndexRelease(uint64_t value) = 0;

    virtual uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value) = 0;

    virtual uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value) = 0;

    virtual uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value) = 0;

    virtual uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value) = 0;

    virtual uint64_t AddWriteIndexAcqRel(uint64_t value) = 0;

    virtual uint64_t AddWriteIndexAcquire(uint64_t value) = 0;

    virtual uint64_t AddWriteIndexRelaxed(uint64_t value) = 0;

    virtual uint64_t AddWriteIndexRelease(uint64_t value) = 0;

    // @brief Submits a block of PM4 and waits until it has been executed.
    // Don't need it in host user mode queue
    // virtual void ExecuteCBUF(uint32_t* cmd_data, size_t cmd_size_b) = 0;
    /*
  virtual void SetProfiling(bool enabled) {
    HCS_BITS_SET(co_queue_.queue_properties, co_queue_PROPERTIES_ENABLE_PROFILING,
                     (enabled != 0));
  }
*/
    // Handle of AMD Queue struct
    co_queue_t& co_queue_;

    // queue_t public_handle() const { return public_handle_; }

    // Base address of queue buffer
    void* queue_address_;

    // Size of queue in terms of Aql Dispatch packets
    uint32_t queue_size_pkts_;

    uint32_t queue_size_bytes_;

protected:
    // StreamPool* GetStreamPool() {return stream_pool_;}

    // StreamPool* stream_pool_;
    /*
  static void set_public_handle(Queue* ptr, queue_t handle) {
    ptr->do_set_public_handle(handle);
  }

  virtual void do_set_public_handle(queue_t handle) {
    public_handle_ = handle;
  }
  */

    // const uint32_t queue_type_;

    // QUEUEID queue_id_;

    // queue_t public_handle_;
    // int doorbell_type_;

    // Indicates if queue is active/inactive
    // std::atomic<bool> active_;

    // Index for the queue object
    // uint64_t queue_index_;

    // Index for the next Aql packet
    // uint64_t next_index_;

    // Queue currently suspended or scheduled
    // bool suspended_;

    // Tracks need for barrier bit based on barrier
    // in signal implementation.
    // bool needs_barrier_;

    // HSA_QUEUE_PRIORITY priority_;

    //Async errors callback
    // EventCallback errors_callback_;

    // Size of queue in terms of Aql Dispatch packets
    // uint32_t queue_size_;

    /// Next available queue id.
    virtual uint64_t GetQueueId() = 0;

    // Used to serialize accesses
    // static KernelMutex queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(IQueue);

private:
    // Shared event used for queue errors
    // static HsaEvent* queue_event_;

    // HSA Queue ID - used to bind a unique ID
    // static std::atomic<uint64_t> queue_counter_;
};

}
