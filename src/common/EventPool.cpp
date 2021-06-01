#include "EventPool.h"

HsaEvent* EventPool::CreateEvent(HSA_EVENTTYPE type, bool manual_reset) {
  HsaEventDescriptor event_descriptor;
  event_descriptor.EventType = type;
  event_descriptor.SyncVar.SyncVar.UserData = NULL;
  event_descriptor.SyncVar.SyncVarSize = sizeof(signal_value_t);
  event_descriptor.NodeId = 0;

  HsaEvent* ret = NULL;
  if (runtime_->CreateEvent(&event_descriptor, manual_reset, false, &ret)) {
    if (type == HSA_EVENTTYPE_MEMORY) {
      memset(&ret->EventData.EventData.MemoryAccessFault.Failure, 0,
             sizeof(HsaAccessAttributeFailure));
    }
  }

  return ret;
}

void EventPool::DestroyEvent(HsaEvent* evt) { /*FIXME runtime_->DestroyEvent(evt);*/ }

HsaEvent* EventPool::alloc() {
  ScopedAcquire<KernelMutex> lock(&lock_);
  if (events_.empty()) {
    if (!allEventsAllocated) {
      HsaEvent* evt = EventPool::GetPool()->CreateEvent(HSA_EVENTTYPE_SIGNAL, false);
      if (evt == nullptr) allEventsAllocated = true;
      return evt;
    }
    return nullptr;
  }
  HsaEvent* ret = events_.back().release();
  events_.pop_back();
  return ret;
}

void EventPool::free(HsaEvent* evt) {
  if (evt == nullptr) return;
  ScopedAcquire<KernelMutex> lock(&lock_);
  events_.push_back(unique_event_ptr(evt));
}


