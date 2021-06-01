#pragma once
// #include "cmake_config.h"
#include <unordered_map>
#include <list>
#include <errno.h>
#include <string.h>
#include <cstdlib>
#include <stdio.h>
#include <atomic>
#include <mutex>
#include <assert.h>
#include <deque>
#include <string>
#include <type_traits>
#include <stddef.h>
#include <exception>
#include <vector>
#include <set>
#include <map>
#include <functional>
#include <chrono>
#include <memory>
#include <queue>
#include <algorithm>

#if defined(__APPLE__) || defined(__FreeBSD__)
# define OS_FreeBSD 1
# define OS_Unix 1
#elif defined(__linux__)
# define OS_Linux 1
# define OS_Unix 1
#elif defined(_WIN32)
# define OS_Windows 1
#endif


#if defined(__GNUC__) && (__GNUC__ > 3 ||(__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
# define ALWAYS_INLINE __attribute__ ((always_inline)) inline
#else
# define ALWAYS_INLINE inline
#endif

#if defined(OS_Unix)
# define LIKELY(x) __builtin_expect(!!(x), 1)
# define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
# define LIKELY(x) x
# define UNLIKELY(x) x
#endif

#if defined(OS_Linux)
# define ATTRIBUTE_WEAK __attribute__((weak))
#elif defined(LIBGO_SYS_FreeBSD)
# define ATTRIBUTE_WEAK __attribute__((weak_import))
#endif

#if defined(OS_Windows)
#pragma warning(disable : 4996)
#endif

#if defined(OS_Windows)
# define FCONTEXT_CALL __stdcall
#else
# define FCONTEXT_CALL
#endif

#if defined(OS_Unix)
#include <unistd.h>
#include <sys/types.h>
#endif

#if defined(OS_Windows)
#include <Winsock2.h>
#include <Windows.h>
#endif

#include <atomic>

namespace co
{

void coInitialize();

template <typename T>
using atomic_t = std::atomic<T>;

// 协程中抛出未捕获异常时的处理方式
enum class eCoExHandle : uint8_t
{
    immedaitely_throw,  // 立即抛出
    on_listener,        // 使用listener处理, 如果没设置listener则立刻抛出
};

typedef void*(*stack_malloc_fn_t)(size_t size);
typedef void(*stack_free_fn_t)(void *ptr);

///---- 配置选项
struct CoroutineOptions
{
    // 调试选项, 例如: dbg_switch 或 dbg_hook|dbg_task|dbg_wait
    // uint64_t debug = 0;
    bool debug(std::string dbg) {
        return false;
    }

    // 调试信息输出位置，改写这个配置项可以重定向输出位置
    FILE* debug_output = stdout;

    // 协程中抛出未捕获异常时的处理方式
    eCoExHandle exception_handle = eCoExHandle::immedaitely_throw;

    // 协程栈大小上限, 只会影响在此值设置之后新创建的P, 建议在首次Run前设置.
    // stack_size建议设置不超过1MB
    // Linux系统下, 设置2MB的stack_size会导致提交内存的使用量比1MB的stack_size多10倍.
    uint32_t stack_size = 1 * 1024 * 1024;

    // 单协程执行超时时长(单位：微秒) (超过时长会强制steal剩余任务, 派发到其他线程)
    uint32_t cycle_timeout_us = 100 * 1000;

    // 调度线程的触发频率(单位：微秒)
    uint32_t dispatcher_thread_cycle_us = 1000;

    // 栈顶设置保护内存段的内存页数量(仅linux下有效)(默认为0, 即:不设置)
    // 在栈顶内存对齐后的前几页设置为protect属性.
    // 所以开启此选项时, stack_size不能少于protect_stack_page+1页
    int & protect_stack_page;

    stack_malloc_fn_t & stack_malloc_fn;
    stack_free_fn_t & stack_free_fn;

    CoroutineOptions();

    ALWAYS_INLINE static CoroutineOptions& getInstance()
    {
        static CoroutineOptions obj;
        return obj;
    }
};


int GetCurrentProcessID();
int GetCurrentThreadID();
int GetCurrentCoroID();
std::string GetCurrentTimeStr();
const char* BaseFile(const char* file);
const char* PollEvent2Str(short int event);
unsigned long NativeThreadID();

#if defined(LIBGO_SYS_Unix)
# define GCC_FORMAT_CHECK __attribute__((format(printf,1,2)))
#else
# define GCC_FORMAT_CHECK
#endif
std::string Format(const char* fmt, ...) GCC_FORMAT_CHECK;
std::string P(const char* fmt, ...) GCC_FORMAT_CHECK;
std::string P();

class ErrnoStore {
public:
    ErrnoStore() : restored_(false) {
#if defined(LIBGO_SYS_Windows)
		wsaErr_ = WSAGetLastError();
#endif
		errno_ = errno;
	}
    ~ErrnoStore() {
        Restore();
    }
    void Restore() {
        if (restored_) return ;
        restored_ = true;
#if defined(LIBGO_SYS_Windows)
		WSASetLastError(wsaErr_);
#endif
        errno = errno_;
    }
private:
    int errno_;
#if defined(LIBGO_SYS_Windows)
	int wsaErr_;
#endif
    bool restored_;
};

extern std::mutex gDbgLock;

} //namespace co
// #define DbgFlag()
#define DebugPrint1(type, fmt, ...) \
    do { \
    } while(0)

#define DebugPrint(type, fmt, ...) \
    do { \
        if (UNLIKELY(::co::CoroutineOptions::getInstance().debug(#type))) { \
            ::co::ErrnoStore es; \
            std::unique_lock<std::mutex> lock(::co::gDbgLock); \
            fprintf(::co::CoroutineOptions::getInstance().debug_output, "[%s][%05d][%04d][%06d]%s:%d:(%s)\t " fmt "\n", \
                    ::co::GetCurrentTimeStr().c_str(),\
                    ::co::GetCurrentProcessID(), ::co::GetCurrentThreadID(), ::co::GetCurrentCoroID(), \
                    ::co::BaseFile(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); \
            fflush(::co::CoroutineOptions::getInstance().debug_output); \
        } \
    } while(0)

#define CO_E2S_DEFINE(x) \
    case x: return #x
