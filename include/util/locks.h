#pragma once

#include "utils.h"
#include "os.h"
#include <atomic>
#include <mutex>
#include <condition_variable>

/// @brief: A class behaves as a lock in a scope. When trying to enter into the
/// critical section, creat a object of this class. After the control path goes
/// out of the scope, it will release the lock automatically.
template <class LockType>
class ScopedAcquire {
 public:
  /// @brief: When constructing, acquire the lock.
  /// @param: lock(Input), pointer to an existing lock.
  explicit ScopedAcquire(LockType* lock) : lock_(lock), doRelease(true) { lock_->Acquire(); }

  /// @brief: when destructing, release the lock.
  ~ScopedAcquire() {
    if (doRelease) lock_->Release();
  }

  /// @brief: Release the lock early.  Avoid using when possible.
  void Release() {
    lock_->Release();
    doRelease = false;
  }

 private:
  LockType* lock_;
  bool doRelease;
  /// @brief: Disable copiable and assignable ability.
  DISALLOW_COPY_AND_ASSIGN(ScopedAcquire);
};

/// @brief: a class represents a kernel mutex.
/// Uses the kernel's scheduler to keep the waiting thread from being scheduled
/// until the lock is released (Best for long waits, though anything using
/// a kernel object is a long wait).
class KernelMutex {
 public:
  KernelMutex() { lock_ = os::CreateMutex(); }
  ~KernelMutex() { os::DestroyMutex(lock_); }

  bool Try() { return os::TryAcquireMutex(lock_); }
  bool Acquire() { return os::AcquireMutex(lock_); }
  void Release() { os::ReleaseMutex(lock_); }

 private:
  os::Mutex lock_;

  /// @brief: Disable copiable and assignable ability.
  DISALLOW_COPY_AND_ASSIGN(KernelMutex);
};

/// @brief: represents a spin lock.
/// For very short hold durations on the order of the thread scheduling
/// quanta or less.
class SpinMutex {
 public:
  SpinMutex() { lock_ = 0; }

  bool Try() {
    int old = 0;
    return lock_.compare_exchange_strong(old, 1);
  }
  bool Acquire() {
    int old = 0;
    while (!lock_.compare_exchange_strong(old, 1))
	{
		old=0;
    os::YieldThread();
	}
    return true;
  }
  void Release() { lock_ = 0; }

 private:
  std::atomic<int> lock_;

  /// @brief: Disable copiable and assignable ability.
  DISALLOW_COPY_AND_ASSIGN(SpinMutex);
};

class KernelEvent {
 public:
  KernelEvent() { evt_ = os::CreateOsEvent(true, true); }
  ~KernelEvent() { os::DestroyOsEvent(evt_); }

  bool IsSet() { return os::WaitForOsEvent(evt_, 0)==0; }
  bool WaitForSet() { return os::WaitForOsEvent(evt_, 0xFFFFFFFF)==0; }
  void Set() { os::SetOsEvent(evt_); }
  void Reset() { os::ResetOsEvent(evt_); }

 private:
  os::EventHandle evt_;

  /// @brief: Disable copiable and assignable ability.
  DISALLOW_COPY_AND_ASSIGN(KernelEvent);
};

template<typename LockType>
class ReaderLockGuard final {
public:
  explicit ReaderLockGuard(LockType &lock):
    lock_(lock)
  {
    lock_.ReaderLock();
  }

  ~ReaderLockGuard()
  {
    lock_.ReaderUnlock();
  }

private:
  ReaderLockGuard(const ReaderLockGuard&);
  ReaderLockGuard& operator=(const ReaderLockGuard&);

  LockType &lock_;
};

template<typename LockType>
class WriterLockGuard final {
public:
  explicit WriterLockGuard(LockType &lock):
    lock_(lock)
  {
    lock_.WriterLock();
  }

  ~WriterLockGuard()
  {
    lock_.WriterUnlock();
  }

private:
  WriterLockGuard(const WriterLockGuard&);
  WriterLockGuard& operator=(const WriterLockGuard&);

  LockType &lock_;
};

class ReaderWriterLock final {
public:
  ReaderWriterLock():
    readers_count_(0), writers_count_(0), writers_waiting_(0) {}

  ~ReaderWriterLock() {}

  void ReaderLock();

  void ReaderUnlock();

  void WriterLock();

  void WriterUnlock();

private:
  ReaderWriterLock(const ReaderWriterLock&);
  ReaderWriterLock& operator=(const ReaderWriterLock&);

  size_t readers_count_;
  size_t writers_count_;
  size_t writers_waiting_;
  std::mutex internal_lock_;
  std::condition_variable_any readers_condition_;
  std::condition_variable_any writers_condition_;
};


