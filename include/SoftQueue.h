#pragma once
#include "core/ISignal.h"
#include "core/IQueue.h"
#include "Shared.h"
#include "Stream.h"

namespace core {

class SoftQueue : public IQueue {
 public:
  explicit SoftQueue(StreamPool* stream_pool, uint32_t ring_size, queue_type32_t type,
                /*uint32_t features,*/ ISignal* doorbell_signal);

  virtual ~SoftQueue();

  status_t Inactivate() override { return SUCCESS; }

  status_t SetPriority(HSA_QUEUE_PRIORITY priority) override {
    return ERROR_INVALID_QUEUE;
  }

  uint64_t LoadReadIndexAcquire() override {
    return atomic_::Load(&co_queue_.read_dispatch_id, std::memory_order_acquire);
  }

  uint64_t LoadReadIndexRelaxed() override {
    return atomic_::Load(&co_queue_.read_dispatch_id, std::memory_order_relaxed);
  }

  uint64_t LoadWriteIndexAcquire() override {
    return atomic_::Load(&co_queue_.write_dispatch_id, std::memory_order_acquire);
  }

  uint64_t LoadWriteIndexRelaxed() override {
    return atomic_::Load(&co_queue_.write_dispatch_id, std::memory_order_relaxed);
  }

  void StoreReadIndexRelaxed(uint64_t value) override {
    atomic_::Store(&co_queue_.read_dispatch_id, value, std::memory_order_relaxed);
  }

  void StoreReadIndexRelease(uint64_t value) override {
    atomic_::Store(&co_queue_.read_dispatch_id, value, std::memory_order_release);
  }

  void StoreWriteIndexRelaxed(uint64_t value) override {
    atomic_::Store(&co_queue_.write_dispatch_id, value, std::memory_order_relaxed);
  }
  void StoreWriteIndexRelease(uint64_t value) override {
    atomic_::Store(&co_queue_.write_dispatch_id, value, std::memory_order_release);
  }

  uint64_t CasWriteIndexAcqRel(uint64_t expected, uint64_t value) override {
    return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
                       std::memory_order_acq_rel);
  }

  uint64_t CasWriteIndexAcquire(uint64_t expected, uint64_t value) override {
    return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
                       std::memory_order_acquire);
  }

  uint64_t CasWriteIndexRelaxed(uint64_t expected, uint64_t value) override {
    return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
                       std::memory_order_relaxed);
  }

  uint64_t CasWriteIndexRelease(uint64_t expected, uint64_t value) override {
    return atomic_::Cas(&co_queue_.write_dispatch_id, value, expected,
                       std::memory_order_release);
  }

  uint64_t AddWriteIndexAcqRel(uint64_t value) override {
    return atomic_::Add(&co_queue_.write_dispatch_id, value,
                       std::memory_order_acq_rel);
  }

  uint64_t AddWriteIndexAcquire(uint64_t value) override {
    return atomic_::Add(&co_queue_.write_dispatch_id, value,
                       std::memory_order_acquire);
  }

  uint64_t AddWriteIndexRelaxed(uint64_t value) override {
    return atomic_::Add(&co_queue_.write_dispatch_id, value,
                       std::memory_order_relaxed);
  }

  uint64_t AddWriteIndexRelease(uint64_t value) override {
    return atomic_::Add(&co_queue_.write_dispatch_id, value,
                       std::memory_order_release);
  }

  void* operator new(size_t size) {
    return _aligned_malloc(size, QUEUE_ALIGN_BYTES);
  }

  void* operator new(size_t size, void* ptr) { return ptr; }

  void operator delete(void* ptr) { _aligned_free(ptr); }

  void operator delete(void*, void*) {}

  uint64_t GetQueueId() { return queue_counter_++;}

  StreamPool* GetStreamPool() {return stream_pool_;}

  StreamPool* stream_pool_;

 private:
  static const size_t kRingAlignment = 256;

  // Host queue id counter, starting from 0x80000000 to avoid overlaping
  // with aql queue id.
  static std::atomic<uint32_t> queue_counter_;

  DISALLOW_COPY_AND_ASSIGN(SoftQueue);

};
}
