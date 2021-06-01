#include "Runtime.h"
#include "EventPool.h"
#include "Stream.h"
#include "core/ISignal.h"

using namespace core;

#if 0
static bool loaded = true;

class RuntimeCleanup {
public:
    ~RuntimeCleanup() {
        if (!Runtime::IsOpen()) {
        }

        loaded = false;
    }
};


static RuntimeCleanup cleanup_at_unload_;

status_t Runtime::Acquire() {
    if (!loaded) return ERROR_OUT_OF_RESOURCES;

    Runtime& runtime = Runtime::getInstance();

    if (runtime.ref_count_ == INT32_MAX) {
        return ERROR_REFCOUNT_OVERFLOW;
    }

    runtime.ref_count_++;
    MAKE_NAMED_SCOPE_GUARD(refGuard, [&]() { runtime.ref_count_--; });

    if (runtime.ref_count_ == 1) {
        status_t status = runtime.Load();

        if (status != SUCCESS) {
            return ERROR_OUT_OF_RESOURCES;
        }
    }

    refGuard.Dismiss();
    return SUCCESS;
}

status_t Runtime::Release() {
    // Check to see if HSA has been cleaned up (process exit)
    if (!loaded) return SUCCESS;

    Runtime& runtime = Runtime::getInstance();

    if (runtime.ref_count_ == 1) {
        // Release all registered memory, then unload backends
        runtime.Unload();
    }

    runtime.ref_count_--;

    return SUCCESS;
}

bool Runtime::IsOpen() {
    Runtime& runtime = Runtime::getInstance();
    return (runtime.ref_count_ != 0);
}
#endif

Runtime::Runtime()
    : sys_clock_freq_(0)
    , ref_count_(0)
{
}


status_t Runtime::AllocateMemory(size_t size, void** address)
{
    *address = malloc(size);
    return SUCCESS;
};

status_t Runtime::FreeMemory(void* address)
{
    free(address);
    return SUCCESS;
};


status_t Runtime::Load()
{
    flag_.Refresh();

    use_interrupt_wait_ = flag_.enable_interrupt();

    // Setup system clock frequency for the first time.
    if (sys_clock_freq_ == 0) {
        // Cache system clock frequency
        // HsaClockCounters clocks;
        // hsaKmtGetClockCounters(0, &clocks);
        // sys_clock_freq_ = clocks.SystemClockFrequencyHz;
    }

    // BindVmFaultHandler();

    return SUCCESS;
}

void Runtime::Unload()
{
    //signal_pool_->clear();
    // event_pool_->clear();
}

const timer::fast_clock::duration Runtime::GetTimeout(double timeout)
{
    uint64_t freq { 0 };
    return timer::duration_from_seconds<timer::fast_clock::duration>(double(timeout) / double(freq));
};

const timer::fast_clock::time_point Runtime::GetTimeNow()
{
    return timer::fast_clock::now();
};

void Runtime::Sleep(uint32_t milisecond)
{
    os::uSleep(20);
};

