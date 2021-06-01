#pragma once
#include "Stream.h"
#include "StreamType.h"
#include "core/IRuntime.h"
#include "util/locks.h"
#include "device_type.h"
#include <memory>
#include <vector>

typedef bool (*signal_handler)(signal_value_t value, void* arg);

namespace core {

class EventPool {
public:
    struct Deleter {
        void operator()(HsaEvent* evt) { reinterpret_cast<EventPool*>(evt->event_pool_)->DestroyEvent(evt); }
    };
    using unique_event_ptr = ::std::unique_ptr<HsaEvent, Deleter>;

    EventPool()
        : allEventsAllocated(false)
    {
    }

    HsaEvent* alloc();
    void free(HsaEvent* evt);
    void clear()
    {
        async_events_control_.Shutdown();
        events_.clear();
        allEventsAllocated = false;
    }

    status_t SetAsyncSignalHandler(ISignal* signal,
        signal_condition_t cond,
        signal_value_t value,
        signal_handler handler,
        void* arg);

    // InterruptSignal::EventPool* GetEventPool() { return &EventPool; }
    void WaitOnEvent(HsaEvent* Event, uint32_t Milliseconds);

    void WaitOnMultipleEvents(HsaEvent* Events[], uint32_t NumEvents,
        bool WaitOnAll, uint32_t Milliseconds);

    HsaEvent* CreateEvent(EVENTTYPE type, bool manual_reset);
    void DestroyEvent(HsaEvent* evt);

    void AsyncEventsLoop(void*);

    StreamPool* GetStreamPool() { return stream_pool_; }

    struct AsyncEventsControl {
        AsyncEventsControl()
            : async_events_thread_(NULL)
        {
        }
        void Shutdown();

        // signal_t wake;
        ISignal* wake;
        os::Thread async_events_thread_;
        KernelMutex lock;
        bool exit;
    };

    struct AsyncEvents {
        void PushBack(ISignal* signal, signal_condition_t cond,
            signal_value_t value, signal_handler handler,
            void* arg);

        void CopyIndex(size_t dst, size_t src);

        size_t Size();

        void PopBack();

        void Clear();

        std::vector<ISignal*> signal_;
        std::vector<signal_condition_t> cond_;
        std::vector<signal_value_t> value_;
        std::vector<signal_handler> handler_;
        std::vector<void*> arg_;
    };

    AsyncEventsControl async_events_control_;
    AsyncEvents async_events_;
    AsyncEvents new_async_events_;
    StreamPool* stream_pool_;

private:
    KernelMutex lock_;
    std::vector<unique_event_ptr> events_;
    bool allEventsAllocated;
};

}
