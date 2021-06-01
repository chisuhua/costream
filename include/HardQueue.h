#pragma once
#include "Shared.h"
#include "Stream.h"
#include "core/IQueue.h"
#include "core/ISignal.h"

namespace core {

using QueueCallback = std::function<void(status_t, IQueue*, void*)>;

class HardQueue : public IQueue, private LocalSignal, public DoorbellSignal {
public:
    explicit HardQueue(StreamPool* stream_pool, IAgent* agent, uint32_t ring_size, queue_type32_t type,
        QueueCallback callback, void* user_data);

    virtual ~HardQueue();
    void Destroy() { delete this; }

    /// @brief Inactivate the queue object. Once inactivate a
    /// queue cannot be used anymore and must be destroyed
    ///
    /// @return hsa_status_t Status of request
    status_t Inactivate() override;

    /// @brief Change the scheduling priority of the queue
    status_t SetPriority(HSA_QUEUE_PRIORITY priority) override;

    void Suspend();

    uint64_t LoadReadIndexAcquire()
    {
        return atomic_::Load(&co_queue_.read_dispatch_id, std::memory_order_acquire);
    }

    uint64_t LoadReadIndexRelaxed()
    {
        return atomic_::Load(&co_queue_.read_dispatch_id, std::memory_order_relaxed);
    }

    uint64_t LoadWriteIndexAcquire()
    {
        return atomic_::Load(&co_queue_.write_dispatch_id, std::memory_order_acquire);
    }

    uint64_t LoadWriteIndexRelaxed()
    {
        return atomic_::Load(&co_queue_.write_dispatch_id, std::memory_order_relaxed);
    }

    void StoreReadIndexRelaxed(uint64_t value) { assert(false); }
    void StoreReadIndexRelease(uint64_t value) { assert(false); }
    void StoreWriteIndexRelaxed(uint64_t value)
    {
        atomic_::Store(&co_queue_.write_dispatch_id, value, std::memory_order_relaxed);
    }
    void StoreWriteIndexRelease(uint64_t value)
    {
        atomic_::Store(&co_queue_.write_dispatch_id, value, std::memory_order_release);
    }

    uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value)
    {
        return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
            std::memory_order_acq_rel);
    }

    uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value)
    {
        return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
            std::memory_order_acquire);
    }

    uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value)
    {
        return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
            std::memory_order_relaxed);
    }

    uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value)
    {
        return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
            std::memory_order_release);
    }

    uint64_t AddWriteIndexAcqRel(uint64_t value)
    {
        return atomic_::Add(&co_queue_.write_dispatch_id, value,
            std::memory_order_acq_rel);
    }

    uint64_t AddWriteIndexAcquire(uint64_t value)
    {
        return atomic_::Add(&co_queue_.write_dispatch_id, value,
            std::memory_order_acquire);
    }

    uint64_t AddWriteIndexRelaxed(uint64_t value)
    {
        return atomic_::Add(&co_queue_.write_dispatch_id, value,
            std::memory_order_relaxed);
    }

    uint64_t AddWriteIndexRelease(uint64_t value)
    {
        return atomic_::Add(&co_queue_.write_dispatch_id, value,
            std::memory_order_release);
    }

    void StoreRelaxed(signal_value_t value)
    {
        if (doorbell_type_ == 2) {
            // Hardware doorbell supports AQL semantics.
            atomic_::Store(co_signal_.hardware_doorbell_ptr, uint64_t(value), std::memory_order_release);
            return;
        }

        // Acquire spinlock protecting the legacy doorbell.
        while (atomic_::Cas(&co_queue_.legacy_doorbell_lock, 1U, 0U,
                   std::memory_order_acquire)
            != 0) {
            os::YieldThread();
        }

#ifdef LARGE_MODEL
        // AMD hardware convention expects the packet index to point beyond
        // the last packet to be processed. Packet indices written to the
        // max_legacy_doorbell_dispatch_id_plus_1 field must conform to this
        // expectation, since this field is used as the HW-visible write index.
        uint64_t legacy_dispatch_id = value + 1;
#else
        // In the small machine model it is difficult to distinguish packet index
        // wrap at 2^32 packets from a backwards doorbell. Instead, ignore the
        // doorbell value and submit the write index instead. It is OK to issue
        // a doorbell for packets in the INVALID or ALWAYS_RESERVED state.
        // The HW will stall on these packets until they enter a valid state.
        uint64_t legacy_dispatch_id = co_queue_.write_dispatch_id;

        // The write index may extend more than a full queue of packets beyond
        // the read index. The hardware can process at most a full queue of packets
        // at a time. Clamp the write index appropriately. A doorbell for the
        // remaining packets is guaranteed to be sent at a later time.
        legacy_dispatch_id = Min(legacy_dispatch_id, uint64_t(co_queue_.read_dispatch_id) + co_queue_.size);
#endif

        // Discard backwards and duplicate doorbells.
        if (legacy_dispatch_id > co_queue_.max_legacy_doorbell_dispatch_id_plus_1) {
            // Record the most recent packet index used in a doorbell submission.
            // This field will be interpreted as a write index upon HW queue connect.
            // Make ring buffer visible to HW before updating write index.
            atomic_::Store(&co_queue_.max_legacy_doorbell_dispatch_id_plus_1,
                legacy_dispatch_id, std::memory_order_release);

            // Write the dispatch id to the hardware MMIO doorbell.
            // Make write index visible to HW before sending doorbell.
            if (doorbell_type_ == 1) {
                atomic_::Store(co_signal_.legacy_hardware_doorbell_ptr,
                    uint32_t(legacy_dispatch_id), std::memory_order_release);
            } else {
                assert(false && "Device has unsupported doorbell semantics");
            }
        }
        // Release spinlock protecting the legacy doorbell.
        // Also ensures timely delivery of (write-combined) doorbell to HW.
        atomic_::Store(&co_queue_.legacy_doorbell_lock, 0U,
            std::memory_order_release);
    };

    void StoreRelease(signal_value_t value)
    {
        std::atomic_thread_fence(std::memory_order_release);
        StoreRelaxed(value);
    }

    // @brief Submits a block of PM4 and waits until it has been executed.
    // Don't need it in host user mode queue
    // virtual void ExecuteCBUF(uint32_t* cmd_data, size_t cmd_size_b) = 0;
    /*
  virtual void SetProfiling(bool enabled) {
    HCS_BITS_SET(co_queue_.queue_properties, co_queue_PROPERTIES_ENABLE_PROFILING,
                     (enabled != 0));
  }
*/
    /// @ brief Reports async queue errors to stderr if no other error handler was registered.
    static void DefaultErrorHandler(status_t status, queue_t* source, void* data);

    // Handle of AMD Queue struct
    //co_queue_t& co_queue_;

    // queue_t public_handle() const { return public_handle_; }

protected:
    /*
  static void set_public_handle(HardQueue* ptr, queue_t handle) {
    ptr->do_set_public_handle(handle);
  }

  virtual void do_set_public_handle(queue_t handle) {
    public_handle_ = handle;
  }
  */

    queue_type_t queue_type_;

    QUEUEID queue_id_;

    queue_t public_handle_;
    int doorbell_type_;

    IAgent* agent_;

    // Indicates if queue is active/inactive
    std::atomic<bool> active_;

    // Index for the queue object
    uint64_t queue_index_;

    // Index for the next Aql packet
    uint64_t next_index_;

    // Queue currently suspended or scheduled
    bool suspended_;

    // Tracks need for barrier bit based on barrier
    // in signal implementation.
    bool needs_barrier_;

    HSA_QUEUE_PRIORITY priority_;

    //Async errors callback
    QueueCallback errors_callback_;

    /// Next available queue id.
    uint64_t GetQueueId() { return queue_counter_++; }

    // Used to serialize accesses
    static KernelMutex queue_lock_;

    DISALLOW_COPY_AND_ASSIGN(HardQueue);

private:
    // Shared event used for queue errors
    static HsaEvent* queue_event_;

    // HSA Queue ID - used to bind a unique ID
    // FIXME
    static std::atomic<uint64_t> queue_counter_;
    // std::atomic<uint64_t> queue_counter_;
};

}
