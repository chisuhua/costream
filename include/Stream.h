#pragma once

#include "StreamType.h"
#include "util/locks.h"
#include <map>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
#include <vector>
// #include "runtime_api.h"

namespace core {
// class Runtime;

// template <typename Allocator = PageAllocator<SharedQueue>>
// class Queue;

class IAgent;
class ISignal;
class IDevice;
class IQueue;
class IRuntime;

class EventPool;
class Context;
class SharedSignalPool;
}
// Alignment attribute that specifies a minimum alignment (in bytes) for
// variables of the specified type.
/*
#if defined(__GNUC__)
#  define __ALIGNED__(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
#  define __ALIGNED__(x) __declspec(align(x))
#elif defined(RC_INVOKED)
#  define __ALIGNED__(x)
#else
#  error
#endif
*/

typedef uint64_t QUEUEID;

// AMD Queue Properties.
typedef uint32_t co_queue_properties32_t;

// TODO schi add for packet callback, it can merge with core::HsaEventCallback in next

namespace core {

typedef void (*Callback)(status_t status, IQueue* source, void* data);

class StreamPool {

public:
    StreamPool();

    status_t AcquireQueue(IAgent* device, uint32_t queue_size_hint, IQueue** queue);
    void ReleaseQueue(IQueue* queue);

    status_t CreateQueue(IAgent* agent, uint32_t size, queue_type32_t type,
        void (*callback)(status_t status, core::IQueue* source, void* data),
        void* data, uint32_t private_segment_size, uint32_t group_segment_size,
        core::IQueue** queue);

    status_t CreateSignal(signal_value_t initial_value, uint32_t num_consumers,
        IAgent** consumers, uint64_t attributes, ISignal** signal);

    //! For the given HSA queue, return an existing hostcall buffer or create a
    //! new one. queuePool_ keeps a mapping from HSA queue to hostcall buffer.
    void* GetOrCreateHostcallBuffer(core::IQueue* queue);

    void DestroyQueue(core::IQueue*);

    void DestroySignal(ISignal* signal_handle);

    /// @brief Waits until any signal in the list satisfies its condition or
    /// timeout is reached.
    /// Returns the index of a satisfied signal.  Returns -1 on timeout and
    /// errors.
    uint32_t WaitAnySignal(uint32_t signal_count, ISignal** signals_,
        const signal_condition_t* conds, const signal_value_t* values,
        uint64_t timeout, wait_state_t wait_hint,
        signal_value_t* satisfying_value);

    Context& GetContext() { return *context_; }

    struct QueueInfo {
        IAgent* agent;
        int ref_count;
        void* hostcallBuffer;
    };

    SharedSignalPool* GetSignalPool() { return signal_pool_; };
    EventPool* GetEventPool() { return event_pool_; };

    IRuntime* GetRuntime() { return runtime_; };
    IDevice* GetDevice() { return device_; };

    using QueuePool_t = std::map<core::IQueue*, QueueInfo>;
    // using DeviceQueue_t = std::map<device_t, QueuePool_t>;

    QueuePool_t queue_pool_;
    SharedSignalPool* signal_pool_;
    EventPool* event_pool_;

    IRuntime* runtime_;
    IDevice* device_;
    Context* context_; //!< A dummy context for internal data transfer

public:
    void RegisterIpc(ISignal* signal);
    bool DeRegisterIpc(ISignal* signal);

    std::map<decltype(signal_t::handle), ISignal*> ipcMap_;
    KernelMutex ipcLock_;

    ISignal* lookupIpc(signal_t signal);
    ISignal* duplicateIpc(signal_t signal);
    ISignal* DuplicateSignalHandle(signal_t signal);
    ISignal* Convert(signal_t signal);

public:
    /// @ brief Reports async queue errors to stderr if no other error handler was registered.
    void DefaultErrorHandler(status_t status, IQueue* source, void* data);
};

}
