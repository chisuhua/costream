#include "Queue.h"


#define QUEUE_LOAD_API(op, order)                                           \
uint64_t QueueLoad##op##Index##order (const queue_t queue_handle) {         \
    core::Queue* queue_obj = core::Queue::Object(queue_handle);             \
    return queue_obj->Load##op##Index##order();                             \
};
QUEUE_LOAD_API(Read, Acquire)
QUEUE_LOAD_API(Read, Relaxed)
QUEUE_LOAD_API(Write, Acquire)
QUEUE_LOAD_API(Write, Relaxed)


#define QUEUE_STORE_API(op, order)                                              \
void QueueStore##op##Index##order (const queue_t queue_handle, uint64_t value) {\
    core::Queue* queue_obj = core::Queue::Object(queue_handle);                 \
    queue_obj->Store##op##Index##order(value);                                  \
};
QUEUE_STORE_API(Write, Relaxed)
QUEUE_STORE_API(Write, Release)
QUEUE_STORE_API(Read, Relaxed)
QUEUE_STORE_API(Read, Release)



