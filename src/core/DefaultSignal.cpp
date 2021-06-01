#include "DefaultSignal.h"
#include "Stream.h"
#include "core/IRuntime.h"
#include "util/timer.h"

// #include "core/util/timer.h"


namespace core {


BusyWaitSignal::BusyWaitSignal(StreamPool* stream_pool, SharedSignal* shared_block, bool enableIPC)
    : ISignal(stream_pool, shared_block, enableIPC) {
        co_signal_.kind = SIGNAL_KIND_USER;
  co_signal_.event_mailbox_ptr = 0;
}

signal_value_t BusyWaitSignal::LoadRelaxed() {
  uint64_t value = atomic_::Load(&co_signal_.value, std::memory_order_relaxed);
  return value;
}

signal_value_t BusyWaitSignal::LoadAcquire() {
  return signal_value_t(atomic_::Load(&co_signal_.value, std::memory_order_acquire));
}

void BusyWaitSignal::StoreRelaxed(signal_value_t value) {
  atomic_::Store(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::StoreRelease(signal_value_t value) {
  atomic_::Store(&co_signal_.value, int64_t(value), std::memory_order_release);
}

signal_value_t BusyWaitSignal::WaitRelaxed(signal_condition_t condition,
                                               signal_value_t compare_value, uint64_t timeout,
                                               wait_state_t wait_hint) {
  Retain();
  MAKE_SCOPE_GUARD([&]() { Release(); });

  waiting_++;
  MAKE_SCOPE_GUARD([&]() { waiting_--; });
  bool condition_met = false;
  int64_t value;

/*
  debug_warning((!g_use_interrupt_wait || isIPC()) &&
                "Use of non-host signal in host signal wait API.");
                */

  timer::fast_clock::time_point start_time, time;
  start_time = timer::fast_clock::now();

  // Set a polling timeout value
  // Should be a few times bigger than null kernel latency
  const timer::fast_clock::duration kMaxElapsed = std::chrono::microseconds(200);

  // uint64_t freq;
  // HSA::system_get_info(SYSTEM_INFO_TIMESTAMP_FREQUENCY, &freq);
  const timer::fast_clock::duration fast_timeout = GetStreamPool()->GetRuntime()->GetTimeout(timeout);
  //    timer::duration_from_seconds<timer::fast_clock::duration>(
  //        double(timeout) / double(freq));

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

    time = GetStreamPool()->GetRuntime()->GetTimeNow();
    if (time - start_time > fast_timeout) {
      value = atomic_::Load(&co_signal_.value, std::memory_order_relaxed);
      return signal_value_t(value);
    }
    if (time - start_time > kMaxElapsed) {
        GetStreamPool()->GetRuntime()->Sleep(20);
    }
  }
}

signal_value_t BusyWaitSignal::WaitAcquire(signal_condition_t condition,
                                               signal_value_t compare_value, uint64_t timeout,
                                               wait_state_t wait_hint) {
  signal_value_t ret =
      WaitRelaxed(condition, compare_value, timeout, wait_hint);
  std::atomic_thread_fence(std::memory_order_acquire);
  return ret;
}

void BusyWaitSignal::AndRelaxed(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::AndAcquire(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::AndRelease(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::AndAcqRel(signal_value_t value) {
  atomic_::And(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::OrRelaxed(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::OrAcquire(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::OrRelease(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::OrAcqRel(signal_value_t value) {
  atomic_::Or(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::XorRelaxed(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::XorAcquire(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::XorRelease(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::XorAcqRel(signal_value_t value) {
  atomic_::Xor(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::AddRelaxed(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::AddAcquire(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::AddRelease(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::AddAcqRel(signal_value_t value) {
  atomic_::Add(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
}

void BusyWaitSignal::SubRelaxed(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_relaxed);
}

void BusyWaitSignal::SubAcquire(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_acquire);
}

void BusyWaitSignal::SubRelease(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_release);
}

void BusyWaitSignal::SubAcqRel(signal_value_t value) {
  atomic_::Sub(&co_signal_.value, int64_t(value), std::memory_order_acq_rel);
}

signal_value_t BusyWaitSignal::ExchRelaxed(signal_value_t value) {
  return signal_value_t(atomic_::Exchange(&co_signal_.value, int64_t(value),
                                             std::memory_order_relaxed));
}

signal_value_t BusyWaitSignal::ExchAcquire(signal_value_t value) {
  return signal_value_t(atomic_::Exchange(&co_signal_.value, int64_t(value),
                                             std::memory_order_acquire));
}

signal_value_t BusyWaitSignal::ExchRelease(signal_value_t value) {
  return signal_value_t(atomic_::Exchange(&co_signal_.value, int64_t(value),
                                             std::memory_order_release));
}

signal_value_t BusyWaitSignal::ExchAcqRel(signal_value_t value) {
  return signal_value_t(atomic_::Exchange(&co_signal_.value, int64_t(value),
                                             std::memory_order_acq_rel));
}

signal_value_t BusyWaitSignal::CasRelaxed(signal_value_t expected,
                                              signal_value_t value) {
  return signal_value_t(atomic_::Cas(&co_signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_relaxed));
}

signal_value_t BusyWaitSignal::CasAcquire(signal_value_t expected,
                                              signal_value_t value) {
  return signal_value_t(atomic_::Cas(&co_signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_acquire));
}

signal_value_t BusyWaitSignal::CasRelease(signal_value_t expected,
                                              signal_value_t value) {
  return signal_value_t(atomic_::Cas(&co_signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_release));
}

signal_value_t BusyWaitSignal::CasAcqRel(signal_value_t expected,
                                             signal_value_t value) {
  return signal_value_t(atomic_::Cas(&co_signal_.value, int64_t(value),
                                        int64_t(expected),
                                        std::memory_order_acq_rel));
}

}  // namespace core

