#include "core/IRuntime.h"
#include "core/IDevice.h"
#include "InterruptSignal.h"
#include "EventPool.h"
#include "Stream.h"
// #include "core/util/timer.h"
// #include "core/util/locks.h"

namespace core {

InterruptSignal::InterruptSignal(StreamPool* stream_pool, signal_value_t initial_value, HsaEvent* use_event)
    : LocalSignal(initial_value, false), ISignal(stream_pool, GetShared()) {
  if (use_event != nullptr) {
    event_ = use_event;
    free_event_ = false;
  } else {
    event_ = stream_pool_->GetEventPool()->alloc();
    free_event_ = true;
  }

  if (event_ != nullptr) {
    co_signal_.event_id = event_->EventId;
    co_signal_.event_mailbox_ptr = event_->EventData.HWData2;
  } else {
    co_signal_.event_id = 0;
    co_signal_.event_mailbox_ptr = 0;
  }
  co_signal_.kind = SIGNAL_KIND_USER;
}


InterruptSignal::~InterruptSignal() {
  if (free_event_) stream_pool_->GetEventPool()->free(event_);
}

signal_value_t InterruptSignal::LoadRelaxed() {
  return signal_value_t(
      atomic_::Load(&co_signal_.value, std::memory_order_relaxed));
}

signal_value_t InterruptSignal::LoadAcquire() {
  return signal_value_t(
      atomic_::Load(&co_signal_.value, std::memory_order_acquire));
}

void InterruptSignal::StoreRelaxed(signal_value_t value) {
  atomic_::Store(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::StoreRelease(signal_value_t value) {
  atomic_::Store(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

signal_value_t InterruptSignal::WaitRelaxed(
    signal_condition_t condition, signal_value_t compare_value,
    uint64_t timeout, wait_state_t wait_hint) {
  Retain();
  MAKE_SCOPE_GUARD([&]() { Release(); });

  uint32_t prior = waiting_++;
  MAKE_SCOPE_GUARD([&]() { waiting_--; });
  // Allow only the first waiter to sleep (temporary, known to be bad).
  if (prior != 0) wait_hint = ACTIVE;

  int64_t value;

  timer::fast_clock::time_point start_time = GetStreamPool()->GetRuntime()->GetTimeNow();

  // Set a polling timeout value
  // Should be a few times bigger than null kernel latency
  const timer::fast_clock::duration kMaxElapsed = std::chrono::microseconds(200);

  const timer::fast_clock::duration fast_timeout = GetStreamPool()->GetRuntime()->GetTimeout(timeout);

  bool condition_met = false;
  while (true) {
    if (!IsValid()) return 0;

    value = atomic_::Load(&co_signal_.value, std::memory_order_relaxed);

    switch (condition) {
      case CONDITION_EQ: {
        condition_met = (value == compare_value);
        break;
      }
      case CONDITION_NE: {
        condition_met = (value != compare_value);
        break;
      }
      case CONDITION_GTE: {
        condition_met = (value >= compare_value);
        break;
      }
      case CONDITION_LT: {
        condition_met = (value < compare_value);
        break;
      }
      default:
        return 0;
    }
    if (condition_met) return signal_value_t(value);

    timer::fast_clock::time_point time = stream_pool_->GetRuntime()->GetTimeNow();
    if (time - start_time > fast_timeout) {
      value = atomic_::Load(&co_signal_.value, std::memory_order_relaxed);
      return signal_value_t(value);
    }

    if (wait_hint == ACTIVE) {
      continue;
    }

    if (time - start_time < kMaxElapsed) {
        stream_pool_->GetRuntime()->Sleep(20);
        continue;
    }

    uint32_t wait_ms;
    auto time_remaining = fast_timeout - (time - start_time);
    uint64_t ct=timer::duration_cast<std::chrono::milliseconds>(time_remaining).count();
    wait_ms = (ct>0xFFFFFFFEu) ? 0xFFFFFFFEu : ct;
    stream_pool_->GetEventPool()->WaitOnEvent(event_, wait_ms);
  }
}

/// @brief Notify driver of signal value change if necessary.
__forceinline void InterruptSignal::SetEvent() {
    std::atomic_signal_fence(std::memory_order_seq_cst);
    if (InWaiting()) stream_pool_->GetDevice()->SetEvent(event_);
}


signal_value_t InterruptSignal::WaitAcquire(
    signal_condition_t condition, signal_value_t compare_value,
    uint64_t timeout, wait_state_t wait_hint) {
  signal_value_t ret = WaitRelaxed(condition, compare_value, timeout, wait_hint);
  std::atomic_thread_fence(std::memory_order_acquire);
  return ret;
}

void InterruptSignal::AndRelaxed(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::AndAcquire(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::AndRelease(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::AndAcqRel(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::OrRelaxed(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::OrAcquire(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::OrRelease(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::OrAcqRel(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::XorRelaxed(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::XorAcquire(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::XorRelease(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::XorAcqRel(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::AddRelaxed(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::AddAcquire(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::AddRelease(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::AddAcqRel(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

void InterruptSignal::SubRelaxed(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
  SetEvent();
}

void InterruptSignal::SubAcquire(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_acquire);
  SetEvent();
}

void InterruptSignal::SubRelease(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_release);
  SetEvent();
}

void InterruptSignal::SubAcqRel(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
  SetEvent();
}

signal_value_t InterruptSignal::ExchRelaxed(signal_value_t value) {
  signal_value_t ret = signal_value_t(atomic_::Exchange(
      &co_signal_.value, int64_t(value), std::memory_order_relaxed));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::ExchAcquire(signal_value_t value) {
  signal_value_t ret = signal_value_t(atomic_::Exchange(
      &co_signal_.value, int64_t(value), std::memory_order_acquire));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::ExchRelease(signal_value_t value) {
  signal_value_t ret = signal_value_t(atomic_::Exchange(
      &co_signal_.value, int64_t(value), std::memory_order_release));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::ExchAcqRel(signal_value_t value) {
  signal_value_t ret = signal_value_t(atomic_::Exchange(
      &co_signal_.value, int64_t(value), std::memory_order_acq_rel));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::CasRelaxed(signal_value_t expected,
                                               signal_value_t value) {
  signal_value_t ret = signal_value_t(
      atomic_::Cas(&co_signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_relaxed));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::CasAcquire(signal_value_t expected,
                                               signal_value_t value) {
  signal_value_t ret = signal_value_t(
      atomic_::Cas(&co_signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_acquire));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::CasRelease(signal_value_t expected,
                                               signal_value_t value) {
  signal_value_t ret = signal_value_t(
      atomic_::Cas(&co_signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_release));
  SetEvent();
  return ret;
}

signal_value_t InterruptSignal::CasAcqRel(signal_value_t expected,
                                              signal_value_t value) {
  signal_value_t ret = signal_value_t(
      atomic_::Cas(&co_signal_.value, int64_t(value), int64_t(expected),
                  std::memory_order_acq_rel));
  SetEvent();
  return ret;
}

}  // namespace core

