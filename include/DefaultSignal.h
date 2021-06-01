#pragma once
#include "core/ISignal.h"

namespace core {
/// @brief Operations for a simple pure memory based signal.
/// @brief See base class Signal.
class BusyWaitSignal : public ISignal {
 public:

  /// @brief See base class Signal.
  explicit BusyWaitSignal(StreamPool* streampool, SharedSignal* abi_block, bool enableIPC);

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

  /// @brief see the base class Signal
  __forceinline signal_value_t* ValueLocation() const {
    return (signal_value_t*)&co_signal_.value;
  }

  /// @brief see the base class Signal
  __forceinline HsaEvent* EopEvent() { return NULL; }


  DISALLOW_COPY_AND_ASSIGN(BusyWaitSignal);
};

/// @brief Simple memory only signal using a new ABI block.
class DefaultSignal : private LocalSignal, public BusyWaitSignal {
 public:

  /// @brief See base class Signal.
  explicit DefaultSignal(StreamPool* stream_pool, signal_value_t initial_value, bool enableIPC = false)
      : LocalSignal(initial_value, enableIPC), BusyWaitSignal(stream_pool, GetShared(), enableIPC) {}

 protected:

  DISALLOW_COPY_AND_ASSIGN(DefaultSignal);
};

}  // namespace core

