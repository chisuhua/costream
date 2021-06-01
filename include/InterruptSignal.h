#pragma once
#include <memory>
#include <vector>

// #include "hsakmt.h"

#include "core/ISignal.h"
// #include "Stream.h"
// #include "core/util/utils.h"

namespace core {

/// @brief A Signal implementation using interrupts versus plain memory based.
/// Also see base class Signal.
///
/// Breaks common/vendor separation - signals in general needs to be re-worked
/// at the foundation level to make sense in a multi-device system.
/// Supports only one waiter for now.
/// KFD changes are needed to support multiple waiters and have device
/// signaling.
class InterruptSignal : private LocalSignal, public ISignal {
 public:

  explicit InterruptSignal(StreamPool* stream_pool, signal_value_t initial_value,
                           HsaEvent* use_event = NULL);

  ~InterruptSignal();

  // Below are various methods corresponding to the APIs, which load/store the
  // signal value or modify the existing signal value automically and with
  // specified memory ordering semantics.

  signal_value_t LoadRelaxed();

  signal_value_t LoadAcquire();

  void StoreRelaxed(signal_value_t value);

  void StoreRelease(signal_value_t value);

  signal_value_t WaitRelaxed(signal_condition_t condition,
                                 signal_value_t compare_value,
                                 uint64_t timeout, wait_state_t wait_hint);

  signal_value_t WaitAcquire(signal_condition_t condition,
                                 signal_value_t compare_value,
                                 uint64_t timeout, wait_state_t wait_hint);

  void AndRelaxed(signal_value_t value);

  void AndAcquire(signal_value_t value);

  void AndRelease(signal_value_t value);

  void AndAcqRel(signal_value_t value);

  void OrRelaxed(signal_value_t value);

  void OrAcquire(signal_value_t value);

  void OrRelease(signal_value_t value);

  void OrAcqRel(signal_value_t value);

  void XorRelaxed(signal_value_t value);

  void XorAcquire(signal_value_t value);

  void XorRelease(signal_value_t value);

  void XorAcqRel(signal_value_t value);

  void AddRelaxed(signal_value_t value);

  void AddAcquire(signal_value_t value);

  void AddRelease(signal_value_t value);

  void AddAcqRel(signal_value_t value);

  void SubRelaxed(signal_value_t value);

  void SubAcquire(signal_value_t value);

  void SubRelease(signal_value_t value);

  void SubAcqRel(signal_value_t value);

  signal_value_t ExchRelaxed(signal_value_t value);

  signal_value_t ExchAcquire(signal_value_t value);

  signal_value_t ExchRelease(signal_value_t value);

  signal_value_t ExchAcqRel(signal_value_t value);

  signal_value_t CasRelaxed(signal_value_t expected,
                                signal_value_t value);

  signal_value_t CasAcquire(signal_value_t expected,
                                signal_value_t value);

  signal_value_t CasRelease(signal_value_t expected,
                                signal_value_t value);

  signal_value_t CasAcqRel(signal_value_t expected,
                               signal_value_t value);

  /// @brief See base class Signal.
  __forceinline signal_value_t* ValueLocation() const {
    return (signal_value_t*)&co_signal_.value;
  }

  /// @brief See base class Signal.
  __forceinline HsaEvent* EopEvent() { return event_; }

 private:
  /// @variable KFD event on which the interrupt signal is based on.
  HsaEvent* event_;

  /// @variable Indicates whether the signal should release the event when it
  /// closes or not.
  bool free_event_;

  /// @brief Notify driver of signal value change if necessary.
  __forceinline void SetEvent();

  DISALLOW_COPY_AND_ASSIGN(InterruptSignal);
};

}  // namespace core

