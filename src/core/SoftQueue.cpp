#include "SoftQueue.h"

#include "core/IRuntime.h"
#include "util/utils.h"

namespace core {

std::atomic<uint32_t> SoftQueue::queue_counter_(0x80000000);

SoftQueue::SoftQueue(StreamPool* stream_pool, uint32_t ring_size, queue_type32_t type,
    /*uint32_t features,*/ ISignal* doorbell_signal)
    : IQueue(ring_size)
    , stream_pool_(stream_pool)
{
    queue_size_bytes_ = queue_size_pkts_ * sizeof(AqlPacket);
    if (SUCCESS != GetStreamPool()->GetRuntime()->AllocateMemory(queue_size_bytes_, &queue_address_)) {
        throw co_exception(ERROR_OUT_OF_RESOURCES, "Soft queue buffer alloc failed\n");
    }
    MAKE_NAMED_SCOPE_GUARD(bufferGuard, [&]() { GetStreamPool()->GetRuntime()->FreeMemory(&queue_address_); });

    assert(isMultipleOf(queue_address_, kRingAlignment));
    assert(queue_address_ != NULL);

    // Fill the ring buffer with invalid packet headers.
    // Leave packet content uninitialized to help track errors.
    for (uint32_t pkt_id = 0; pkt_id < queue_size_pkts_; pkt_id++) {
        (((AqlPacket*)queue_address_)[pkt_id]).dispatch.header = PACKET_TYPE_INVALID;
    }


    co_queue_.base_address = queue_address_;
    co_queue_.size = queue_size_pkts_;
    co_queue_.doorbell_signal = ISignal::Handle(doorbell_signal);
    co_queue_.id = this->GetQueueId();
    co_queue_.type = type;
    // co_queue_.features = features;
#ifdef HSA_LARGE_MODEL
    BITS_SET(co_queue_.queue_properties, QUEUE_PROPERTIES_IS_PTR64, 1);
#else
    BITS_SET(co_queue_.queue_properties, QUEUE_PROPERTIES_IS_PTR64, 0);
#endif
    co_queue_.write_dispatch_id = co_queue_.read_dispatch_id = 0;
    BITS_SET(co_queue_.queue_properties, QUEUE_PROPERTIES_ENABLE_PROFILING, 0);

    bufferGuard.Dismiss();
    // registerGuard.Dismiss();
}

SoftQueue::~SoftQueue()
{
    GetStreamPool()->GetRuntime()->FreeMemory(queue_address_);
    // GetStreamPool()->GetRuntime()->memory_deregister(this, sizeof(SoftQueue));
}

} // namespace core

