// #include "Stream.h"
#include "core/ISignal.h"
#include "DefaultSignal.h"
#include "EventPool.h"
#include "InterruptSignal.h"
#include "core/IRuntime.h"

#include <algorithm>
// #include "core/util/timer.h"
//#include "core/inc/runtime.h"

namespace core {

// KernelMutex ISignal::ipcLock_;
// std::map<decltype(signal_t::handle), ISignal*> ISignal::ipcMap_;

void SharedSignalPool::clear()
{
    ifdebug
    {
        size_t capacity = 0;
        for (auto& block : block_list_)
            capacity += block.second;
        if (capacity != free_list_.size())
            debug_print("Warning: Resource leak detected by SharedSignalPool, %ld Signals leaked.\n",
                capacity - free_list_.size());
    }

    for (auto& block : block_list_)
        free_(block.first);
    block_list_.clear();
    free_list_.clear();
}

SharedSignal* SharedSignalPool::alloc()
{
    ScopedAcquire<KernelMutex> lock(&lock_);
    if (free_list_.empty()) {
        SharedSignal* block = reinterpret_cast<SharedSignal*>(
            allocate_(block_size_ * sizeof(SharedSignal), __alignof(SharedSignal), 0));
        if (block == nullptr) {
            block_size_ = minblock_;
            block = reinterpret_cast<SharedSignal*>(
                allocate_(block_size_ * sizeof(SharedSignal), __alignof(SharedSignal), 0));
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

    SharedSignal* ret = free_list_.back();
    new (ret) SharedSignal();
    free_list_.pop_back();
    return ret;
}

void SharedSignalPool::free(SharedSignal* ptr)
{
    if (ptr == nullptr) return;

    ptr->~SharedSignal();
    ScopedAcquire<KernelMutex> lock(&lock_);

    ifdebug
    {
        bool valid = false;
        for (auto& block : block_list_) {
            if ((block.first <= ptr) && (uintptr_t(ptr) < uintptr_t(block.first) + block.second * sizeof(SharedSignal))) {
                valid = true;
                break;
            }
        }
        assert(valid && "Object does not belong to pool.");
    }

    free_list_.push_back(ptr);
}

LocalSignal::LocalSignal(signal_value_t initial_value, bool exportable)
    /* FIXME : local_signal_(exportable ? nullptr
                               : core::Runtime::runtime_singleton_->GetSharedSignalPool(),
                               */
    : local_signal_(nullptr, 0)
{
    local_signal_.shared_object()->co_signal_.value = initial_value;
}

void ISignal::registerIpc()
{
    /*
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    auto handle = Handle(this);
    assert(ipcMap_.find(handle.handle) == ipcMap_.end() && "Can't register the same IPC signal twice.");
    ipcMap_[handle.handle] = this;
    */
    GetStreamPool()->RegisterIpc(this);
}

bool ISignal::deregisterIpc()
{
    /*
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    if (refcount_ != 0) return false;
    auto handle = Handle(this);
    const auto& it = ipcMap_.find(handle.handle);
    assert(it != ipcMap_.end() && "Deregister on non-IPC signal.");
    ipcMap_.erase(it);
    return true;
    */
    return GetStreamPool()->DeRegisterIpc(this);
}

ISignal* ISignal::DuplicateHandle(signal_t signal)
{
    return GetStreamPool()->DuplicateSignalHandle(signal);
}

void ISignal::Release()
{
    if (--retained_ != 0) return;
    if (!isIPC())
        doDestroySignal();
    else if (deregisterIpc())
        doDestroySignal();
}

ISignal::~ISignal()
{
    co_signal_.kind = SIGNAL_KIND_INVALID;
    if (refcount_ == 1 && isIPC()) {
        refcount_ = 0;
        deregisterIpc();
    }
}

SignalGroup::SignalGroup(uint32_t num_signals, const signal_t* signals)
    : count(num_signals)
{
    if (count != 0) {
        signals_ = new signal_t[count];
    } else {
        signals_ = NULL;
    }
    if (signals_ == NULL) return;
    for (uint32_t i = 0; i < count; i++)
        signals_[i] = signals[i];
}

} // namespace core

