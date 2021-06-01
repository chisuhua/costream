#pragma once

class Device;
class Runtime;

#include "stream_api.h"


#define SignalForward(func)                          \
    template <typename... Args>                      \
    void func(signal_t signal, Args&&... arg);

#define SignalStaticForward(func)                    \
    template <typename... Args>                      \
    void func(Args&&... arg);


#define SIGNAL_WAIT_ARGS              \
    signal_t signal,                  \
        signal_condition_t condition, \
        signal_value_t compare_value, \
        uint64_t timeout_hint,        \
        wait_state_t wait_state_hint

#define SIGNAL_GROUP_WAIT_ARGS         \
    signal_group_t signal_group,       \
        signal_condition_t *condition, \
        signal_value_t *compare_value, \
        wait_state_t wait_state_hint,  \
        signal_t *signal,              \
        signal_value_t *value

template<typename Runtime, typename Device>
class CoStream {
public:
    Runtime* runtime_;
    Device* device_;

    SignalForward(SubRelease)
    SignalForward(WaitRelaxed)
    SignalForward(EopEvent)
    SignalStaticForward(WaitAny)

    status_t signal_create(
            signal_value_t initial_value,
            uint32_t num_consumers,
            const device_t* consumers,
            signal_t* signal,
            HsaEvent* event = nullptr
            );

    status_t signal_destroy(signal_t signal);
    status_t signal_load_scacquire(signal_t signal);
    status_t signal_store_screlease(signal_t signal);
    status_t signal_load_relaxed(signal_t signal);
    status_t signal_store_relaxed(signal_t signal, signal_value_t);
    status_t signal_or_relaxed(signal_t signal, signal_value_t signal_value);

    status_t signal_wait_scacquire(SIGNAL_WAIT_ARGS);
    status_t signal_wait_relaxed(SIGNAL_WAIT_ARGS);

    status_t signal_group_create(
        uint32_t num_signals,
        const signal_t* signals,
        uint32_t num_consumers,
        const device_t* consumers,
        signal_group_t* signal_group);

    status_t signal_group_destroy(signal_group_t signal_group);

    status_t signal_group_wait_any_scacquire(SIGNAL_GROUP_WAIT_ARGS);
    status_t signal_group_wait_any_relaxed(SIGNAL_GROUP_WAIT_ARGS);

    status_t queue_create(
        Device* device,
        uint32_t size,
        queue_type32_t type,
        void (*callback)(status_t status, queue_t* source, void* data),
        void* data,
        uint32_t private_segment_size,
        uint32_t group_segment_size,
        queue_t** queue);

    status_t queue_destroy(queue_t* queue);

#if 0
#define STREAM_API(type, name) \
    void type##_##name(type##_##name##_args* args);

#include "stream_api.inc"
#undef STREAM_API

#define QUEUE_LOAD_API(op, order) \
    void queue_load_##op##order(queue_load_##op##order##_args* args);

#define QUEUE_STORE_API(op, order) \
    void queue_store_##op##order(queue_store_##op##order##_args* args);

#define QUEUE_OP_API(op, order) \
    void queue_##op##_wptr_##order(queue_##op##_wptr_##order##_args* args);

#include "stream_queue_api.inc"
#undef QUEUE_LOAD_API
#undef QUEUE_STORE_API
#undef QUEUE_OP_API
#endif
};
