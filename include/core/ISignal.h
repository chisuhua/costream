#pragma once

#include <atomic>
#include <map>
#include <set>
#include <vector>
// #include "utils/lang/error.h"

// Need HsaEvent
// #include "inc/hsakmt.h"
// #include "EventPool.h"
// #include "inc/hsakmttypes.h"
#include "StreamType.h"
#include "device_type.h"
#include "stream_api.h"

#include "Shared.h"

// #include "inc/platform.h"
// schi #include "core/inc/checked.h"
#include "exceptions.h"

#include "util/atomic_helpers.h"
#include "util/locks.h"
#include "util/utils.h"

// #include "Stream.h"
#include <algorithm>

namespace core {

class IRuntime;
class IAgent;
class StreamPool;

// Allow signal_t to be keys in STL structures.
/*
namespace std {
template <> struct less<signal_t> {
  bool operator()(const signal_t& x, const signal_t& y) const {
    return x.handle < y.handle;
  }
  typedef signal_t first_argument_type;
  typedef signal_t second_argument_type;
  typedef bool result_type;
};
}
*/

class ISignal;

/// @brief ABI and object conversion struct for signals.  May be shared between processes.
struct SharedSignal {
    co_signal_t co_signal_;
    ISignal* core_signal;

    SharedSignal()
    {
        memset(&co_signal_, 0, sizeof(co_signal_));
        co_signal_.kind = SIGNAL_KIND_INVALID;
        core_signal = nullptr;
    }

    // bool IsValid() const { return (Convert(this).handle != 0) && id.IsValid(); }
    bool IsValid() const { return true; }

    bool IsIPC() const { return core_signal == nullptr; }

    static __forceinline SharedSignal* Object(signal_t signal)
    {
        SharedSignal* ret = reinterpret_cast<SharedSignal*>(static_cast<uintptr_t>(signal.handle) - offsetof(SharedSignal, co_signal_));
        return ret;
    }

    static __forceinline signal_t Handle(const SharedSignal* shared_signal)
    {
        assert(shared_signal != nullptr && "Conversion on null Signal object.");
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&shared_signal->co_signal_));
        const signal_t signal_handle = { handle };
        return signal_handle;
    }
};
/*
static_assert(std::is_standard_layout<SharedSignal>::value,
              "SharedSignal must remain standard layout for IPC use.");
static_assert(std::is_trivially_destructible<SharedSignal>::value,
              "SharedSignal must not be modified on delete for IPC use.");
              */
/// @brief Pool class for SharedSignal suitable for use with Shared.
class SharedSignalPool : private BaseShared {
public:
    SharedSignalPool()
        : block_size_(minblock_)
    {
    }
    ~SharedSignalPool() { clear(); }

    SharedSignal* alloc();
    void free(SharedSignal* ptr);
    void clear();

private:
    static const size_t minblock_ = 4096 / sizeof(SharedSignal);
    KernelMutex lock_;
    std::vector<SharedSignal*> free_list_;
    std::vector<std::pair<void*, size_t>> block_list_;
    size_t block_size_;
};

class LocalSignal {
public:
    // Temporary, for legacy tools lib support.
    explicit LocalSignal(signal_value_t initial_value)
    {
        local_signal_.shared_object()->co_signal_.value = initial_value;
    }
    LocalSignal(signal_value_t initial_value, bool exportable);

    SharedSignal* GetShared() const { return local_signal_.shared_object(); }
    SharedSignal* GetSharedSignal() const { return local_signal_.shared_object(); }

private:
    Shared<SharedSignal, SharedSignalPool> local_signal_;
};

/// @brief An abstract base class which helps implement the public signal_t
/// type (an opaque handle) and its associated APIs. At its core, signal uses
/// a 32 or 64 bit value. This value can be waitied on or signaled atomically
/// using specified memory ordering semantics.
class ISignal {
public:
    /// @brief Constructor Links and publishes the signal interface object.
    explicit ISignal(StreamPool* stream_pool, SharedSignal* shared_signal, bool enableIPC = false)
        : stream_pool_(stream_pool)
        , co_signal_(shared_signal->co_signal_)
        , async_copy_agent_(NULL)
        , refcount_(1)
    {
        assert(shared_signal != nullptr && "Signal shared_signal must not be NULL");

        waiting_ = 0;
        retained_ = 1;

        if (enableIPC) {
            shared_signal->core_signal = nullptr;
            registerIpc();
        } else {
            shared_signal->core_signal = this;
        }
    }

    /// @brief Interface to discard a signal handle (signal_t)
    /// Decrements signal ref count and invokes doDestroySignal() when
    /// Signal is no longer in use.
    void DestroySignal()
    {
        // If handle is now invalid wake any retained sleepers.
        if (--refcount_ == 0) CasRelaxed(0, 0);
        // Release signal, last release will destroy the object.
        Release();
    }

    /// @brief Converts from this interface class to the public
    /// signal_t type - an opaque handle.
    static signal_t Handle(ISignal* core_signal)
    {
        assert(core_signal != nullptr && "Conversion on null Signal object.");
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&core_signal->co_signal_));
        const signal_t signal_handle = { handle };
        return signal_handle;
    }

    /// @brief Converts from this interface class to the public
    /// signal_t type - an opaque handle.
    static const signal_t Handle(const ISignal* core_signal)
    {
        assert(core_signal != nullptr && "Conversion on null Signal object.");
        const uint64_t handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&core_signal->co_signal_));
        const signal_t signal_handle = { handle };
        return signal_handle;
    }


    ISignal* DuplicateHandle(signal_t signal);

    bool IsValid() const { return refcount_ != 0; }

    bool isIPC() const { return SharedSignal::Object(Handle(this))->IsIPC(); }

    // Below are various methods corresponding to the APIs, which load/store the
    // signal value or modify the existing signal value automically and with
    // specified memory ordering semantics.
    virtual signal_value_t LoadRelaxed() = 0;
    virtual signal_value_t LoadAcquire() = 0;

    virtual void StoreRelaxed(signal_value_t value) = 0;
    virtual void StoreRelease(signal_value_t value) = 0;

    virtual signal_value_t WaitRelaxed(signal_condition_t condition,
        signal_value_t compare_value,
        uint64_t timeout,
        wait_state_t wait_hint)
        = 0;
    virtual signal_value_t WaitAcquire(signal_condition_t condition,
        signal_value_t compare_value,
        uint64_t timeout,
        wait_state_t wait_hint)
        = 0;

    virtual void AndRelaxed(signal_value_t value) = 0;
    virtual void AndAcquire(signal_value_t value) = 0;
    virtual void AndRelease(signal_value_t value) = 0;
    virtual void AndAcqRel(signal_value_t value) = 0;

    virtual void OrRelaxed(signal_value_t value) = 0;
    virtual void OrAcquire(signal_value_t value) = 0;
    virtual void OrRelease(signal_value_t value) = 0;
    virtual void OrAcqRel(signal_value_t value) = 0;

    virtual void XorRelaxed(signal_value_t value) = 0;
    virtual void XorAcquire(signal_value_t value) = 0;
    virtual void XorRelease(signal_value_t value) = 0;
    virtual void XorAcqRel(signal_value_t value) = 0;

    virtual void AddRelaxed(signal_value_t value) = 0;
    virtual void AddAcquire(signal_value_t value) = 0;
    virtual void AddRelease(signal_value_t value) = 0;
    virtual void AddAcqRel(signal_value_t value) = 0;

    virtual void SubRelaxed(signal_value_t value) = 0;
    virtual void SubAcquire(signal_value_t value) = 0;
    virtual void SubRelease(signal_value_t value) = 0;
    virtual void SubAcqRel(signal_value_t value) = 0;

    virtual signal_value_t ExchRelaxed(signal_value_t value) = 0;
    virtual signal_value_t ExchAcquire(signal_value_t value) = 0;
    virtual signal_value_t ExchRelease(signal_value_t value) = 0;
    virtual signal_value_t ExchAcqRel(signal_value_t value) = 0;

    virtual signal_value_t CasRelaxed(signal_value_t expected,
        signal_value_t value)
        = 0;
    virtual signal_value_t CasAcquire(signal_value_t expected,
        signal_value_t value)
        = 0;
    virtual signal_value_t CasRelease(signal_value_t expected,
        signal_value_t value)
        = 0;
    virtual signal_value_t CasAcqRel(signal_value_t expected,
        signal_value_t value)
        = 0;

    /// @brief Returns the address of the value.
    virtual signal_value_t* ValueLocation() const = 0;

    /// @brief Applies only to InterrupEvent type, returns the event used to.
    /// Returns NULL for DefaultEvent Type.
    virtual HsaEvent* EopEvent() = 0;

    /// @brief Prevents the signal from being destroyed until the matching Release().
    void Retain() { retained_++; }
    void Release();

    /// @brief Checks if signal is currently in use by a wait API.
    bool InWaiting() const { return waiting_ != 0; }

    void async_copy_agent(IAgent* agent)
    {
        async_copy_agent_ = agent;
    }

    IAgent* async_copy_agent() { return async_copy_agent_; }

    /// @brief Structure which defines key signal elements like type and value.
    /// Address of this struct is used as a value for the opaque handle of type
    /// signal_t provided to the public API.
    co_signal_t& co_signal_;

    /// @variable Indicates number of runtime threads waiting on this signal.
    /// Value of zero means no waits.
    std::atomic<uint32_t> waiting_;

    /// @variable Ref count of this signal's handle (see IPC APIs)
    std::atomic<uint32_t> refcount_;
protected:
    virtual ~ISignal();

    /// @brief Overrideable deletion function
    virtual void doDestroySignal() { delete this; }

    StreamPool* GetStreamPool() { return stream_pool_; }

    /// @variable Pointer to device used to perform an async copy.
    IAgent* async_copy_agent_;

    StreamPool* stream_pool_;

private:
    /*
  static KernelMutex ipcLock_;
  static std::map<decltype(signal_t::handle), ISignal*> ipcMap_;

  static ISignal* lookupIpc(signal_t signal);
  static ISignal* duplicateIpc(signal_t signal);
  */


    /// @variable Count of handle references and Retain() calls for this handle (see IPC APIs)
    std::atomic<uint32_t> retained_;

    void registerIpc();
    bool deregisterIpc();

    DISALLOW_COPY_AND_ASSIGN(ISignal);
};

/// @brief Handle signal operations which are not for use on doorbells.
class DoorbellSignal : public ISignal {
public:
    using ISignal::ISignal;

    /// @brief This operation is illegal
    signal_value_t LoadRelaxed() final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t LoadAcquire() final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t WaitRelaxed(signal_condition_t condition, signal_value_t compare_value,
        uint64_t timeout, wait_state_t wait_hint) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t WaitAcquire(signal_condition_t condition, signal_value_t compare_value,
        uint64_t timeout, wait_state_t wait_hint) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    void AndRelaxed(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AndAcquire(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AndRelease(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AndAcqRel(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void OrRelaxed(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void OrAcquire(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void OrRelease(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void OrAcqRel(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void XorRelaxed(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void XorAcquire(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void XorRelease(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void XorAcqRel(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AddRelaxed(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AddAcquire(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AddRelease(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void AddAcqRel(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void SubRelaxed(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void SubAcquire(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void SubRelease(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    void SubAcqRel(signal_value_t value) final override { assert(false); }

    /// @brief This operation is illegal
    signal_value_t ExchRelaxed(signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t ExchAcquire(signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t ExchRelease(signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t ExchAcqRel(signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t CasRelaxed(signal_value_t expected,
        signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t CasAcquire(signal_value_t expected,
        signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t CasRelease(signal_value_t expected,
        signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t CasAcqRel(signal_value_t expected,
        signal_value_t value) final override
    {
        assert(false);
        return 0;
    }

    /// @brief This operation is illegal
    signal_value_t* ValueLocation() const final override
    {
        assert(false);
        return NULL;
    }

    /// @brief This operation is illegal
    HsaEvent* EopEvent() final override
    {
        assert(false);
        return NULL;
    }

protected:
    /// @brief Disallow destroying doorbell apart from its queue.
    void doDestroySignal() final override { assert(false); }
};

#if 0
struct signal_handle {
    signal_t signal;

    signal_handle() { }
    signal_handle(signal_t Signal) { signal = Signal; }
    operator signal_t() { return signal; }
    ISignal* operator->() { return ISignal::Object(signal); }
};

static_assert(
    sizeof(signal_handle) == sizeof(signal_t),
    "signal_handle and signal_t must have identical binary layout.");
static_assert(
    sizeof(signal_handle[2]) == sizeof(signal_t[2]),
    "signal_handle and signal_t must have identical binary layout.");
#endif

class SignalGroup {
public:
    static signal_group_t Convert(SignalGroup* group)
    {
        const signal_group_t handle = { static_cast<uint64_t>(reinterpret_cast<uintptr_t>(group)) };
        return handle;
    }
    static SignalGroup* Convert(signal_group_t group)
    {
        return reinterpret_cast<SignalGroup*>(static_cast<uintptr_t>(group.handle));
    }

    SignalGroup(uint32_t num_signals, const signal_t* signals);
    ~SignalGroup() { delete[] signals_; }

    bool IsValid() const
    {
        // if (CheckedType::IsValid() && signals != NULL) return true;
        // return false;
        return true;
    }

    const signal_t* List() const { return signals_; }
    uint32_t Count() const { return count; }

private:
    signal_t* signals_;
    const uint32_t count;
    DISALLOW_COPY_AND_ASSIGN(SignalGroup);
};
class SignalDeleter {
public:
    void operator()(ISignal* ptr) { ptr->DestroySignal(); }
};
using unique_signal_ptr = ::std::unique_ptr<ISignal, SignalDeleter>;

}
