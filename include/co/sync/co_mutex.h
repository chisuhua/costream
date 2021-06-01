#pragma once
#include "common/inc/OsSupport.h"
#include "processor/Processor.h"
#include <queue>
#include "co/sync/co_condition_variable.h"

namespace co
{

/// 协程锁
class CoMutex
{
    typedef std::mutex lock_t;
    lock_t lock_;
    bool notified_ = false;
    ConditionVariableAny cv_;

    std::atomic_long sem_;

public:
    CoMutex();
    ~CoMutex();

    void lock();
    bool try_lock();
    bool is_lock();
    void unlock();
};

typedef CoMutex co_mutex;

} //namespace co
