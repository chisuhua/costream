#include "core/IQueue.h"
#include "core/IRuntime.h"

namespace core {

// HSA Queue ID - used to bind a unique ID
// std::atomic<uint64_t> Queue::queue_counter_{0};
// HsaEvent* Queue::queue_event_{nullptr};


// Queue allocator used insteaf of PageALlocator, use inside soc
void SharedQueuePool::clear() {
  ifdebug {
    size_t capacity = 0;
    for (auto& block : block_list_) capacity += block.second;
    if (capacity != free_list_.size())
      debug_print("Warning: Resource leak detected by SharedQueuePool, %ld Queues leaked.\n",
                  capacity - free_list_.size());
  }

  for (auto& block : block_list_) free_(block.first);
  block_list_.clear();
  free_list_.clear();
}

SharedQueue* SharedQueuePool::alloc() {
  ScopedAcquire<KernelMutex> lock(&lock_);
  if (free_list_.empty()) {
    SharedQueue* block = reinterpret_cast<SharedQueue*>(
        allocate_(block_size_ * sizeof(SharedQueue), __alignof(SharedQueue), 0));
    if (block == nullptr) {
      block_size_ = minblock_;
      block = reinterpret_cast<SharedQueue*>(
          allocate_(block_size_ * sizeof(SharedQueue), __alignof(SharedQueue), 0));
      if (block == nullptr) throw std::bad_alloc();
    }

    MAKE_NAMED_SCOPE_GUARD(throwGuard, [&]() { free_(block); });
    block_list_.push_back(std::make_pair(block, block_size_));
    throwGuard.Dismiss();

    for (size_t i = 0; i < block_size_; i++) {
      free_list_.push_back(&block[i]);
    }

    block_size_ *= 2;
  }

  SharedQueue* ret = free_list_.back();
  new (ret) SharedQueue();
  free_list_.pop_back();
  return ret;
}

void SharedQueuePool::free(SharedQueue* ptr) {
  if (ptr == nullptr) return;

  ptr->~SharedQueue();
  ScopedAcquire<KernelMutex> lock(&lock_);

  ifdebug {
    bool valid = false;
    for (auto& block : block_list_) {
      if ((block.first <= ptr) &&
          (uintptr_t(ptr) < uintptr_t(block.first) + block.second * sizeof(SharedQueue))) {
        valid = true;
        break;
      }
    }
    assert(valid && "Object does not belong to pool.");
  }

  free_list_.push_back(ptr);
}

}  // namespace core

