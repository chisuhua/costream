#include "HardQueue.h"
#include "DefaultSignal.h"
#include "EventPool.h"
#include "InterruptSignal.h"
#include "Stream.h"
#include "core/IAgent.h"
#include "core/IDevice.h"
#include "core/IRuntime.h"
#include "core/ISignal.h"
#include "exceptions.h"

namespace core {
// extern std::function<void*(size_t, size_t)> g_queue_allocator;
// extern std::function<void(void*)> g_queue_deallocator;
std::atomic<uint64_t> HardQueue::queue_counter_ { 0 };

constexpr uint64_t min_bytes = 0x400;
constexpr uint64_t max_bytes = 0x100000000;

HardQueue::HardQueue(StreamPool* stream_pool, IAgent* agent, uint32_t ring_size, queue_type32_t type,
    QueueCallback callback, void* user_data)
    : IQueue(ring_size)
    , LocalSignal(0, false)
    , DoorbellSignal(stream_pool, GetSharedSignal())
    , agent_(agent)
    , queue_type_(QUEUE_TYPE_MULTI)
    , active_(true)
    , next_index_(0)
    , needs_barrier_(true)
    , errors_callback_(callback)

{
    queue_address_ = NULL;
    doorbell_type_ = agent->GetCapability().ui32.DoorbellType;

    const uint32_t min_pkts = (min_bytes / sizeof(AqlPacket));
    const uint32_t max_pkts = (max_bytes / sizeof(AqlPacket));

    queue_size_pkts_ = Min(queue_size_pkts_, max_pkts);
    queue_size_pkts_ = Max(queue_size_pkts_, min_pkts);

    queue_size_bytes_ = queue_size_pkts_ * sizeof(AqlPacket);

    if ((queue_size_bytes_ & (queue_size_bytes_ - 1)) != 0)
        throw co_exception(ERROR_INVALID_QUEUE_CREATION, "Requested queue with non-power of two packet capacity.\n");

    queue_address_ = GetStreamPool()->GetDevice()->QueueBufferAllocator(queue_size_bytes_, HSA_PACKET_ALIGN_BYTES);

    if (queue_address_ == NULL) { throw std::bad_alloc(); }

    MAKE_NAMED_SCOPE_GUARD(bufferGuard, [&]() {
        GetStreamPool()->GetDevice()->QueueBufferDeallocator(queue_address_);
        queue_address_ = NULL;
    });

    assert(isMultipleOf(queue_address_, HSA_PACKET_ALIGN_BYTES));

    // Initialize queue buffer with invalid packet.
    const uint16_t kInvalidHeader = (PACKET_TYPE_INVALID << PACKET_HEADER_TYPE);

    for (size_t i = 0; i < queue_size_pkts_; ++i) {
        reinterpret_cast<AqlPacket*>(queue_address_)[i].device.header = kInvalidHeader;
    }

    // Zero the co_queue_ structure to clear RPTR/WPTR before queue attach.
    memset(&co_queue_, 0, sizeof(co_queue_));

    // Initialize and map a HW AQL queue.
    QueueResource queue_rsrc = { 0 };
    queue_rsrc.Queue_read_ptr_aql = (uint64_t*)&co_queue_.read_dispatch_id;

    if (doorbell_type_ == 2) {
        // Hardware write pointer supports AQL semantics.
        queue_rsrc.Queue_write_ptr_aql = (uint64_t*)&co_queue_.write_dispatch_id;
    } else {
        // Map hardware write pointer to a software proxy.
        queue_rsrc.Queue_write_ptr_aql = (uint64_t*)&co_queue_.max_legacy_doorbell_dispatch_id_plus_1;
    }

    // Populate co_queue_ structure.
    co_queue_.type = QUEUE_TYPE_MULTI;
    co_queue_.features = QUEUE_FEATURE_KERNEL_DISPATCH;
    co_queue_.base_address = queue_address_;
    co_queue_.doorbell_signal = ISignal::Handle(this);
    co_queue_.size = queue_size_pkts_;
    // co_queue_.hsa_queue.id = GetID(this);
    co_queue_.write_dispatch_id = 0;
    co_queue_.read_dispatch_id = 0;
    co_queue_.read_dispatch_id_field_base_byte_offset = uint32_t(
        uintptr_t(&co_queue_.read_dispatch_id) - uintptr_t(&co_queue_));
    BITS_SET(co_queue_.queue_properties, QUEUE_PROPERTIES_ENABLE_PROFILING, 0);
    BITS_SET(co_queue_.queue_properties, QUEUE_PROPERTIES_IS_PTR64, (sizeof(void*) == 8));

    // Populate doorbell signal structure.
    memset(&co_signal_, 0, sizeof(co_signal_));
    co_signal_.kind = (doorbell_type_ == 2) ? SIGNAL_KIND_DOORBELL : SIGNAL_KIND_LEGACY_DOORBELL;
    co_signal_.legacy_hardware_doorbell_ptr = nullptr;
    co_signal_.queue_ptr = &co_queue_;

    // Bind the index of queue just created
    // queue_index_ = queue_cnt_;
    // queue_cnt_++;

    //device_status_t device_status;
    /*device_status = */
    GetStreamPool()->GetDevice()->CreateQueue(agent->node_id(), HSA_QUEUE_COMPUTE, 100, HSA_QUEUE_PRIORITY_MAXIMUM, queue_address_, queue_size_pkts_, NULL, &queue_rsrc);
    //if (device_status != DEVICE_STATUS_SUCCESS)
    //throw exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES, "Queue create failed at hsaKmtCreateQueue\n");

    co_signal_.legacy_hardware_doorbell_ptr = (volatile uint32_t*)queue_rsrc.Queue_DoorBell;

    // Bind Id of Queue such that is unique i.e. it is not re-used by another
    // queue (AQL, HOST) in the same process during its lifetime.
    co_queue_.id = this->GetQueueId();

    queue_id_ = queue_rsrc.QueueId;

    MAKE_NAMED_SCOPE_GUARD(QueueGuard, [&]() { GetStreamPool()->GetDevice()->DestroyQueue(queue_id_); });
#if 0
    MAKE_NAMED_SCOPE_GUARD(EventGuard, [&]() {
        ScopedAcquire<KernelMutex> _lock(queue_lock_);
        queue_counter_--;
        if (queue_counter_ == 0) {
            GetStreamPool()->.GetEventPool()->DestroyEvent(queue_event_);
            queue_event_ = nullptr;
        }
    });
#endif
    MAKE_NAMED_SCOPE_GUARD(SignalGuard, [&]() {
        // FIXME GetStreamPool()->.GetSignalPool()->DestroySignal(co_queue_.queue_inactive_signal);
    });

    if (GetStreamPool()->GetRuntime()->use_interrupt_wait_) {
#if 0
        ScopedAcquire<KernelMutex> _lock(&queue_lock_);
        queue_counter_++;
        if (queue_event_ == nullptr) {
            assert(queue_counter_ == 1 && "Inconsistency in queue event reference counting found.\n");

            queue_event_ = GetStreamPool()->.GetEventPool()->CreateEvent(EVENTTYPE_SIGNAL, false);
            if (queue_event_ == nullptr)
                throw co_exception(ERROR, "Queue event creation failed.\n");
        }
        auto Signal = new InterruptSignal(0, queue_event_);
        assert(Signal != nullptr && "Should have thrown!\n");
        co_queue_.queue_inactive_signal = core::InterruptSignal::Handle(Signal);
#endif
    } else {
        // EventGuard.Dismiss();
        auto Signal = new DefaultSignal(stream_pool_, 0);
        assert(Signal != nullptr && "Should have thrown!\n");
        co_queue_.queue_inactive_signal = core::DefaultSignal::Handle(Signal);
    }

    // Initialize scratch memory related entities
    // queue_scratch_.queue_retry = co_queue_.queue_inactive_signal;
    /*
    if (hsa_amd_signal_async_handler(co_queue_.queue_inactive_signal, HSA_SIGNAL_CONDITION_NE,
                                    0, DynamicScratchHandler, this) != HSA_STATUS_SUCCESS)
        throw hcs::hsa_exception(HSA_STATUS_ERROR_OUT_OF_RESOURCES,
                         "Queue event handler failed registration.\n");
                         */
    active_ = true;
    bufferGuard.Dismiss();
    QueueGuard.Dismiss();
    // EventGuard.Dismiss();
    SignalGuard.Dismiss();
}

HardQueue::~HardQueue()
{
    // Remove error handler synchronously.
    // Sequences error handler callbacks with queue destroy.
    /* dynamicScratchState |= ERROR_HANDLER_TERMINATE;
  runtime_->hsa_signal_store_screlease(co_queue_.queue_inactive_signal, 0x8000000000000000ull);
  runtime_->SignalStoreScrelease(co_queue_.queue_inactive_signal, 0x8000000000000000ull);
  while ((dynamicScratchState & ERROR_HANDLER_DONE) != ERROR_HANDLER_DONE) {
    hsa_signal_wait_relaxed(co_queue_.queue_inactive_signal, HSA_SIGNAL_CONDITION_NE,
                                 0x8000000000000000ull, -1ull, HSA_WAIT_STATE_BLOCKED);
    hsa_signal_store_relaxed(co_queue_.queue_inactive_signal, 0x8000000000000000ull);
  }*/

    // Prevent further use of the queue
    Inactivate();

    // agent_->ReleaseQueueScratch(queue_scratch_);
    // FIXME GetStreamPool()->GetSignalPool()->DestroySignal(co_queue_.queue_inactive_signal);

    if (GetStreamPool()->GetRuntime()->use_interrupt_wait_) {
#if 0
    ScopedAcquire<KernelMutex> lock(&queue_lock_);
    queue_counter_--;
    if (queue_counter_ == 0) {
      GetStreamPool()->GetEventPool()->DestroyEvent(queue_event_);
      queue_event_ = nullptr;
    }
#endif
    }

    // Check for no AQL queue (indicates abort of queue creation).
    if (!IsValid()) return;

    // drain hw queue
    // FlushDevice();

    // close hw queue.
    GetStreamPool()->GetDevice()->DestroyQueue(queue_id_);
    /*
  hsaKmtDestroyQueue(queue_resource_.QueueId);
  hsaKmtUnmapMemoryToGPU(queue_address_);
  hsaKmtFreeMemory(queue_address_, kCBUFQueueSize);
  */

    // Release AQL queue
    // agent_->QueueDeallocator(queue_address_);

    queue_address_ = NULL;

    if (co_queue_.type == QUEUE_TYPE_COOPERATIVE) {
        // agent_->GWSRelease();
        return;
    }
}

void HardQueue::Suspend()
{
    suspended_ = true;
    /*
  auto err = hsaKmtUpdateQueue(queue_id_, 0, priority_, ring_buf_, ring_buf_alloc_bytes_, NULL);
  assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtUpdateQueue failed.");
  */
}

status_t HardQueue::Inactivate()
{
    bool active = active_.exchange(false, std::memory_order_relaxed);
    if (active) {
        GetStreamPool()->GetDevice()->DestroyQueue(queue_id_);
        /*
    auto err = hsaKmtDestroyQueue(queue_id_);
    assert(err == HSAKMT_STATUS_SUCCESS && "hsaKmtDestroyQueue failed.");
    */
        atomic_::Fence(std::memory_order_acquire);
    }
    return SUCCESS;
}

status_t HardQueue::SetPriority(HSA_QUEUE_PRIORITY priority)
{
    if (suspended_) {
        return ERROR;
    }

    priority_ = priority;
    //auto err = hsaKmtUpdateQueue(queue_id_, 100, priority_, ring_buf_, ring_buf_alloc_bytes_, NULL);
    //return (err == HSAKMT_STATUS_SUCCESS ? HSA_STATUS_SUCCESS : HSA_STATUS_ERROR_OUT_OF_RESOURCES);
    return SUCCESS;
}

}
