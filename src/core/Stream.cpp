#include "Stream.h"
#include "DefaultSignal.h"
#include "EventPool.h"
#include "HardQueue.h"
#include "InterruptSignal.h"
#include "SoftQueue.h"
#include "core/IAgent.h"
#include "core/IQueue.h"
#include "core/IRuntime.h"
#include "core/ISignal.h"
#include "drive_type.h"

using namespace core;

StreamPool::StreamPool() {};

status_t StreamPool::AcquireQueue(IAgent* agent, uint32_t queue_size_hint, IQueue** queue)
{
    for (auto& itr : queue_pool_) {
        if (itr.second.agent != agent) continue;
        if (itr.second.ref_count > 0) continue;
        *queue = itr.first;
        return SUCCESS;
    }

    uint32_t queue_max_packets = 0;
    if (SUCCESS != agent->GetInfo(HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_max_packets)) {
        return ERROR;
    }

    auto queue_size = (queue_max_packets < queue_size_hint) ? queue_max_packets : queue_size_hint;
    IQueue* ret_queue;
    status_t status = CreateQueue(agent, queue_size, QUEUE_TYPE_MULTI, nullptr, nullptr, 0, 0, &ret_queue);
    if (status != SUCCESS) return status;

    *queue = ret_queue;

    auto result = queue_pool_.emplace(QueuePool_t::value_type { *queue, QueueInfo() });
    // assert(result.second && "QueueInfo alreadyexists");
    auto& qInfo = result.first->second;
    qInfo.ref_count = 1;
    return SUCCESS;
};

void StreamPool::ReleaseQueue(IQueue* queue)
{
    auto iter = queue_pool_.find(queue);
    assert(iter != queue_pool_.end());

    auto& qInfo = iter->second;
    assert(qInfo.ref_count > 0);
    qInfo.ref_count--;
    if (qInfo.ref_count != 0) { return; }

    if (qInfo.hostcallBuffer) {
        // disableHostcalls(qInfo.hostcallBuffer_, queue);
        // TODO Context().svmFree(qInfo.hostcallBuffer_);
    }

    DestroyQueue(queue);
    queue_pool_.erase(iter);
}

void* StreamPool::GetOrCreateHostcallBuffer(core::IQueue* queue)
{
#if 0
  auto qIter = queuePool_.find(queue);
  assert(qIter != queuePool_.end());

  auto& qInfo = qIter->second;
  if (qInfo.hostcallBuffer_) {
    return qInfo.hostcallBuffer_;
  }

  // The number of packets required in each buffer is at least equal to the
  // maximum number of waves supported by the device.
  auto wavesPerCu = info().maxThreadsPerCU_ / info().wavefrontWidth_;
  auto numPackets = info().maxComputeUnits_ * wavesPerCu;

  auto size = getHostcallBufferSize(numPackets);
  auto align = getHostcallBufferAlignment();

  void* buffer = context().svmAlloc(size, align, CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS);
  if (!buffer) {
    ClPrint(amd::LOG_ERROR, amd::LOG_QUEUE,
            "Failed to create hostcall buffer for hardware queue %p", queue);
    return nullptr;
  }
  ClPrint(amd::LOG_INFO, amd::LOG_QUEUE, "Created hostcall buffer %p for hardware queue %p", buffer,
          queue);
  qInfo.hostcallBuffer_ = buffer;
  if (!enableHostcalls(buffer, numPackets, queue)) {
    ClPrint(amd::LOG_ERROR, amd::LOG_QUEUE, "Failed to register hostcall buffer %p with listener",
            buffer);
    return nullptr;
  }
  return buffer;
#endif
}

status_t StreamPool::CreateQueue(
    IAgent* agent, uint32_t size, queue_type32_t type,
    void (*callback)(status_t status, IQueue* source, void* data),
    void* data, uint32_t private_segment_size, uint32_t group_segment_size,
    IQueue** queue)
{
    if ((size == 0) || (!IsPowerOfTwo(size)) || (type > QUEUE_TYPE_COOPERATIVE)) {
        return ERROR_INVALID_ARGUMENT;
    }

    IQueue* queue_obj;
    if (agent != 0) {
        queue_type32_t agent_queue_type;
        agent->GetInfo(HSA_AGENT_INFO_QUEUE_TYPE, &agent_queue_type);

        assert((agent_queue_type == QUEUE_TYPE_SINGLE) && (type != QUEUE_TYPE_SINGLE));

        QueueCallback queue_callback;
        if (callback == nullptr) {
            queue_callback = [this](status_t status, IQueue* s, void* d) {
                this->DefaultErrorHandler(status, s, d);
            };
        } else {
            queue_callback = callback;
        }
        queue_obj = new core::HardQueue(this, agent, size, type, queue_callback, data);
    } else {
        ISignal* signal;
        CreateSignal(0, 0, NULL, 0, &signal);
        queue_obj = new core::SoftQueue(this, size, type, /*features,*/ signal);
    }

    *queue = queue_obj;

    return SUCCESS;
}

void StreamPool::DestroyQueue(IQueue* queue)
{
    queue->Destroy();
};

/*
static Queue* Queue::Create(queue_t* queue_handle) {
    assert(queue_handle != nullptr);
    Queue* queue = Queue::Object(queue_handle);
    IS_VALID(queue);
    queue->Destroy();
}
*/
void DestroySignal(ISignal* signal)
{
    // core::Signal* signal = core::Signal::Object(signal_handle);
    signal->DestroySignal();
}

status_t StreamPool::CreateSignal(
    signal_value_t initial_value, uint32_t num_consumers,
    IAgent** consumers, uint64_t attributes, ISignal** signal)
{
    bool enable_ipc = attributes & SIGNAL_IPC;
    bool use_default = enable_ipc || (attributes & SIGNAL_GPU_ONLY) || (!GetRuntime()->use_interrupt_wait_);

    if ((!use_default) && (num_consumers != 0)) {
        // IS_BAD_PTR(consumers);

        // Check for duplicates in consumers.
        std::set<IAgent*> consumer_set(consumers, consumers + num_consumers);
        if (consumer_set.size() != num_consumers) {
            return ERROR_INVALID_ARGUMENT;
        }

        use_default = true;
        for (IAgent* cpu_agent : GetRuntime()->GetCpuAgents()) {
            // any consumer is cpu agent, we can use InterruptSignal
            use_default &= (consumer_set.find(cpu_agent) == consumer_set.end());
        }
    }

    ISignal* ret;
    if (use_default) {
        ret = new DefaultSignal(this, initial_value, enable_ipc);
    } else {
        ret = new InterruptSignal(this, initial_value);
    }

    *signal = ret;
    return SUCCESS;
};

uint32_t StreamPool::WaitAnySignal(uint32_t signal_count, ISignal** signals,
    const signal_condition_t* conds, const signal_value_t* values,
    uint64_t timeout, wait_state_t wait_hint,
    signal_value_t* satisfying_value)
{
    // signal_handle* signals = reinterpret_cast<signal_handle*>(const_cast<signal_t*>(signals_));

    for (uint32_t i = 0; i < signal_count; i++)
        signals[i]->Retain();

    MAKE_SCOPE_GUARD([&]() {
        for (uint32_t i = 0; i < signal_count; i++)
            signals[i]->Release();
    });

    uint32_t prior = 0;
    for (uint32_t i = 0; i < signal_count; i++)
        prior = Max(prior, signals[i]->waiting_++);

    MAKE_SCOPE_GUARD([&]() {
        for (uint32_t i = 0; i < signal_count; i++)
            signals[i]->waiting_--;
    });

    // Allow only the first waiter to sleep (temporary, known to be bad).
    if (prior != 0) wait_hint = ACTIVE;

    // Ensure that all signals in the list can be slept on.
    if (wait_hint != ACTIVE) {
        for (uint32_t i = 0; i < signal_count; i++) {
            if (signals[i]->EopEvent() == NULL) {
                wait_hint = ACTIVE;
                break;
            }
        }
    }

    const uint32_t small_size = 10;
    HsaEvent* short_evts[small_size];
    HsaEvent** evts = NULL;
    uint32_t unique_evts = 0;
    if (wait_hint != ACTIVE) {
        if (signal_count > small_size)
            evts = new HsaEvent*[signal_count];
        else
            evts = short_evts;
        for (uint32_t i = 0; i < signal_count; i++)
            evts[i] = signals[i]->EopEvent();
        std::sort(evts, evts + signal_count);
        HsaEvent** end = std::unique(evts, evts + signal_count);
        unique_evts = uint32_t(end - evts);
    }
    MAKE_SCOPE_GUARD([&]() {
        if (signal_count > small_size) delete[] evts;
    });

    int64_t value;

    timer::fast_clock::time_point start_time = timer::fast_clock::now();

    // Set a polling timeout value
    const timer::fast_clock::duration kMaxElapsed = std::chrono::microseconds(200);

    // Convert timeout value into the fast_clock domain
    const timer::fast_clock::duration fast_timeout = GetRuntime()->GetTimeout(timeout);

    bool condition_met = false;
    while (true) {
        for (uint32_t i = 0; i < signal_count; i++) {
            if (!signals[i]->IsValid()) return uint32_t(-1);

            // Handling special event.
            if (signals[i]->EopEvent() != NULL) {
                const EVENTTYPE event_type = signals[i]->EopEvent()->EventData.EventType;
                if (event_type == EVENTTYPE_MEMORY) {
                    const HsaMemoryAccessFault& fault = signals[i]->EopEvent()->EventData.EventData.MemoryAccessFault;
                    if (fault.Flags == EVENTID_MEMORY_FATAL_PROCESS) {
                        return i;
                    }
                }
            }

            value = atomic_::Load(&signals[i]->co_signal_.value, std::memory_order_relaxed);

            switch (conds[i]) {
            case CONDITION_EQ: {
                condition_met = (value == values[i]);
                break;
            }
            case CONDITION_NE: {
                condition_met = (value != values[i]);
                break;
            }
            case CONDITION_GTE: {
                condition_met = (value >= values[i]);
                break;
            }
            case CONDITION_LT: {
                condition_met = (value < values[i]);
                break;
            }
            default:
                return uint32_t(-1);
            }
            if (condition_met) {
                if (satisfying_value != NULL) *satisfying_value = value;
                return i;
            }
        }

        timer::fast_clock::time_point time = timer::fast_clock::now();
        if (time - start_time > fast_timeout) {
            return uint32_t(-1);
        }

        if (wait_hint == ACTIVE) {
            continue;
        }

        if (time - start_time < kMaxElapsed) {
            //  os::uSleep(20);
            continue;
        }

        uint32_t wait_ms;
        auto time_remaining = fast_timeout - (time - start_time);
        uint64_t ct = timer::duration_cast<std::chrono::milliseconds>(
            time_remaining)
                          .count();
        wait_ms = (ct > 0xFFFFFFFEu) ? 0xFFFFFFFEu : ct;
        GetEventPool()->WaitOnMultipleEvents(evts, unique_evts, false, wait_ms);
    }
}

void StreamPool::RegisterIpc(ISignal* signal)
{
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    auto handle = ISignal::Handle(signal);
    assert(ipcMap_.find(handle.handle) == ipcMap_.end() && "Can't register the same IPC signal twice.");
    ipcMap_[handle.handle] = signal;
}

bool StreamPool::DeRegisterIpc(ISignal* signal)
{
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    if (signal->refcount_ != 0) return false;
    auto handle = ISignal::Handle(signal);
    const auto& it = ipcMap_.find(handle.handle);
    assert(it != ipcMap_.end() && "Deregister on non-IPC signal.");
    ipcMap_.erase(it);
    return true;
}

ISignal* StreamPool::lookupIpc(signal_t signal)
{
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    const auto& it = ipcMap_.find(signal.handle);
    if (it == ipcMap_.end()) return nullptr;
    return it->second;
}

ISignal* StreamPool::duplicateIpc(signal_t signal)
{
    ScopedAcquire<KernelMutex> lock(&ipcLock_);
    const auto& it = ipcMap_.find(signal.handle);
    if (it == ipcMap_.end()) return nullptr;
    it->second->refcount_++;
    it->second->Retain();
    return it->second;
}

ISignal* StreamPool::DuplicateSignalHandle(signal_t signal)
{
    if (signal.handle == 0) return nullptr;
    SharedSignal* shared = SharedSignal::Object(signal);

    if (!shared->IsIPC()) {
        if (!shared->IsValid()) return nullptr;
        shared->core_signal->refcount_++;
        shared->core_signal->Retain();
        return shared->core_signal;
    }

    // IPC signals may only be duplicated while holding the ipcMap lock.
    return duplicateIpc(signal);
}

/// @brief Converts from public signal_t type (an opaque handle) to
/// this interface class object.
ISignal* StreamPool::Convert(signal_t signal)
{
    if (signal.handle == 0) {
        assert("Signal handle is invalid");
    }
    SharedSignal* shared = SharedSignal::Object(signal);

    if (!shared->IsValid()) {
        assert("Signal handle is invalid");
    }

    if (shared->IsIPC()) {
        ISignal* ret = lookupIpc(signal);
        if (ret == nullptr) {
            assert("Signal handle is invalid");
        }
        return ret;
    } else {
        return shared->core_signal;
    }
}

void StreamPool::DefaultErrorHandler(status_t status, IQueue* source, void* data) {
  if (GetRuntime()->flag().enable_queue_fault_message()) {
    const char* msg = "UNKNOWN ERROR";
    // HSA::hsa_status_string(status, &msg);
    fprintf(stderr, "Queue at %p inactivated due to async error:\n\t%s\n", source, msg);
  }
}

