#include "EventPool.h"
#include "StreamType.h"
#include "core/IDevice.h"
#include "core/ISignal.h"
#include <cstring>

using namespace core;

HsaEvent* EventPool::alloc()
{
    ScopedAcquire<KernelMutex> lock(&lock_);
    if (events_.empty()) {
        if (!allEventsAllocated) {
            HsaEvent* evt = CreateEvent(EVENTTYPE_SIGNAL, false);
            if (evt == nullptr) allEventsAllocated = true;
            return evt;
        }
        return nullptr;
    }
    HsaEvent* ret = events_.back().release();
    events_.pop_back();
    return ret;
}

void EventPool::free(HsaEvent* evt)
{
    if (evt == nullptr) return;
    ScopedAcquire<KernelMutex> lock(&lock_);
    events_.push_back(unique_event_ptr(evt));
}

HsaEvent* EventPool::CreateEvent(EVENTTYPE type, bool manual_reset)
{
    HsaEventDescriptor event_descriptor;
    event_descriptor.EventType = type;
    event_descriptor.SyncVar.SyncVar.UserData = NULL;
    event_descriptor.SyncVar.SyncVarSize = sizeof(signal_value_t);
    event_descriptor.NodeId = 0;

    HsaEvent* ret = NULL;
    if (SUCCESS == GetStreamPool()->GetDevice()->CreateEvent(&event_descriptor, manual_reset, false, &ret)) {
        if (type == EVENTTYPE_MEMORY) {
            memset(&ret->EventData.EventData.MemoryAccessFault.Failure, 0,
                sizeof(HsaAccessAttributeFailure));
        }
    }

    return ret;
}

void EventPool::DestroyEvent(HsaEvent* evt)
{
    GetStreamPool()->GetDevice()->DestroyEvent(evt);
}

void EventPool::WaitOnEvent(HsaEvent* evt, uint32_t milli_seconds)
{
    GetStreamPool()->GetDevice()->WaitOnEvent(evt, milli_seconds);
};

void EventPool::WaitOnMultipleEvents(HsaEvent* evts[], uint32_t num_evts, bool wait_on_all, uint32_t milli_seconds)
{
    GetStreamPool()->GetDevice()->WaitOnMultipleEvents(evts, num_evts, wait_on_all, milli_seconds);
};

status_t EventPool::SetAsyncSignalHandler(ISignal* signal,
    signal_condition_t cond,
    signal_value_t value,
    signal_handler handler,
    void* arg)
{
    // Indicate that this signal is in use.
    // if (signal.handle != 0) signal_handle(signal)->Retain();
    if (signal != nullptr) signal->Retain();

    ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);

    // Lazy initializer
    if (async_events_control_.async_events_thread_ == NULL) {
        // Create monitoring thread control signal
        // async_events_control_.wake = GetStreamPool()->CreateSignal(0, 0, NULL, 0);
        GetStreamPool()->CreateSignal(0, 0, NULL, 0, &(async_events_control_.wake));

        async_events_.PushBack(async_events_control_.wake, CONDITION_NE,
            0, NULL, NULL);

        // Start event monitoring thread
        async_events_control_.exit = false;
        //FIXME async_events_control_.async_events_thread_ =
        //    os::CreateThread(AsyncEventsLoop, NULL);
        if (async_events_control_.async_events_thread_ == NULL) {
            assert(false && "Asyncronous events thread creation error.");
            return ERROR_OUT_OF_RESOURCES;
        }
    }

    new_async_events_.PushBack(signal, cond, value, handler, arg);

    // signal_handle(async_events_control_.wake)->StoreRelease(1);
    (async_events_control_.wake)->StoreRelease(1);

    return SUCCESS;
}

void EventPool::AsyncEventsLoop(void*)
{
    while (!async_events_control_.exit) {
        // Wait for a signal
        signal_value_t value;
        uint32_t index = GetStreamPool()->WaitAnySignal(
            uint32_t(async_events_.Size()), &async_events_.signal_[0],
            &async_events_.cond_[0], &async_events_.value_[0], uint64_t(-1),
            BLOCKED, &value);

        // Reset the control signal
        if (index == 0) {
            (async_events_control_.wake)->StoreRelaxed(0);
        } else if (index != -1) {
            // No error or timout occured, process the handler
            assert(async_events_.handler_[index] != NULL);
            bool keep = async_events_.handler_[index](value, async_events_.arg_[index]);
            if (!keep) {
                (async_events_.signal_[index])->Release();
                async_events_.CopyIndex(index, async_events_.Size() - 1);
                async_events_.PopBack();
            }
        }

        // Check for dead signals
        index = 0;
        while (index != async_events_.Size()) {
            if (!(async_events_.signal_[index])->IsValid()) {
                (async_events_.signal_[index])->Release();
                async_events_.CopyIndex(index, async_events_.Size() - 1);
                async_events_.PopBack();
                continue;
            }
            index++;
        }

        // Insert new signals and find plain functions
        typedef std::pair<void (*)(void*), void*> func_arg_t;
        std::vector<func_arg_t> functions;
        {
            ScopedAcquire<KernelMutex> scope_lock(&async_events_control_.lock);
            for (size_t i = 0; i < new_async_events_.Size(); i++) {
                if (new_async_events_.signal_[i] == nullptr) {
                    functions.push_back(
                        func_arg_t((void (*)(void*))new_async_events_.handler_[i],
                            new_async_events_.arg_[i]));
                    continue;
                }
                async_events_.PushBack(
                    new_async_events_.signal_[i], new_async_events_.cond_[i],
                    new_async_events_.value_[i], new_async_events_.handler_[i],
                    new_async_events_.arg_[i]);
            }
            new_async_events_.Clear();
        }

        // Call plain functions
        for (size_t i = 0; i < functions.size(); i++)
            functions[i].first(functions[i].second);
        functions.clear();
    }

    // Release wait count of all pending signals
    for (size_t i = 1; i < async_events_.Size(); i++)
        (async_events_.signal_[i])->Release();
    async_events_.Clear();

    for (size_t i = 0; i < new_async_events_.Size(); i++)
        (new_async_events_.signal_[i])->Release();
    new_async_events_.Clear();
}

void EventPool::AsyncEventsControl::Shutdown()
{
    if (async_events_thread_ != NULL) {
        exit = true;
        wake->StoreRelaxed(1);
        os::WaitForThread(async_events_thread_);
        os::CloseThread(async_events_thread_);
        async_events_thread_ = NULL;
        // GetStreamPool()->GetRuntime()->GetSignalPool()->DestroySignal(wake);

        wake->DestroySignal();
        // core::Signal::DestroySignal(wake);
        // HSA::hsa_signal_destroy(wake);
    }
}

void EventPool::AsyncEvents::PushBack(ISignal* signal,
    signal_condition_t cond,
    signal_value_t value,
    signal_handler handler, void* arg)
{
    signal_.push_back(signal);
    cond_.push_back(cond);
    value_.push_back(value);
    handler_.push_back(handler);
    arg_.push_back(arg);
}

void EventPool::AsyncEvents::CopyIndex(size_t dst, size_t src)
{
    signal_[dst] = signal_[src];
    cond_[dst] = cond_[src];
    value_[dst] = value_[src];
    handler_[dst] = handler_[src];
    arg_[dst] = arg_[src];
}

size_t EventPool::AsyncEvents::Size()
{
    return signal_.size();
}

void EventPool::AsyncEvents::PopBack()
{
    signal_.pop_back();
    cond_.pop_back();
    value_.pop_back();
    handler_.pop_back();
    arg_.pop_back();
}

void EventPool::AsyncEvents::Clear()
{
    signal_.clear();
    cond_.clear();
    value_.clear();
    handler_.clear();
    arg_.clear();
}

