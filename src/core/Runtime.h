#pragma once

#include "core/IRuntime.h"

class Runtime : public core::IRuntime {
    public:
    Runtime();
    Runtime(const Runtime&);
    Runtime& operator=(const Runtime&);
    ~Runtime() {}

    virtual status_t AllocateMemory(size_t size, void** address) override;
    virtual status_t FreeMemory(void* ptr) override;
    virtual status_t CopyMemory(void* dst, const void* src, size_t size) override;

    virtual const timer::fast_clock::duration GetTimeout(double timeout) override;
    virtual const timer::fast_clock::time_point GetTimeNow() override;
    virtual void Sleep(uint32_t milisecond) override;

    status_t Load() ;
    virtual void Unload() ;

    uint64_t sys_clock_freq_;
    std::atomic<uint32_t> ref_count_;
};
