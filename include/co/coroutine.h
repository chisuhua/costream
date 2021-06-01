#pragma once
#include "common/inc/OsSupport.h"
#include "co/pp.h"
#include "co/syntax_helper.h"
#include "co/sync/channel.h"
#include "co/sync/co_mutex.h"
#include "co/sync/co_rwmutex.h"
#include "common/inc/Timer.h"
#include "processor/Processor.h"
#include "processor/CoLocalStorage.h"
#include "pool/ConnectionPool.h"
#include "pool/AsyncCoroutinePool.h"
#include "defer/Defer.h"
#include "debug/Listener.h"
#include "debug/CoDebugger.h"
#include "timer/Timer.h"

#define LIBGO_VERSION 300

#define go_alias ::co::__go(__FILE__, __LINE__)-
#define go go_alias

// create coroutine options
#define co_stack(size) ::co::__go_option<::co::opt_stack_size>{size}-
#define co_scheduler(pScheduler) ::co::__go_option<::co::opt_scheduler>{pScheduler}-

#define go_stack(size) go co_stack(size)

#define co_yield do { ::co::Processor::StaticCoYield(); } while (0)

// coroutine sleep, never blocks current thread if run in coroutine.
#if defined(LIBGO_SYS_Unix)
// # define co_sleep(milliseconds) do { usleep(1000 * milliseconds); } while (0)
# define co_sleep(milliseconds) do {  \
    this::this_thread::sleep_for(std::chrono::microseconds(milliseconds)); } \
    while (0)
#else
# define co_sleep(milliseconds) do { ::sleep(milliseconds); } while (0)
#endif

// co_sched
#define co_sched g_Scheduler

#define co_opt ::co::CoroutineOptions::getInstance()

// co_mutex
using ::co::co_mutex;

// co_rwmutex
using ::co::co_rwmutex;
using ::co::co_rmutex;
using ::co::co_wmutex;

// co_chan
using ::co::co_chan;

// co_timer
typedef ::co::CoTimer co_timer;
typedef ::co::CoTimer::TimerId co_timer_id;

//// co_await
//#define co_await(type) ::co::__async_wait<type>()-

//// co_debugger
//#define co_debugger ::co::CoDebugger::getInstance()

// coroutine local storage
#define co_cls(type, ...) CLS(type, ##__VA_ARGS__)
#define co_cls_ref(type) CLS_REF(type)

// co_defer
#define co_defer auto LIBGO_PP_CAT(__defer_, __COUNTER__) = ::co::__defer_op()-
#define co_last_defer() ::co::GetLastDefer()
#define co_defer_scope co_defer [&]
