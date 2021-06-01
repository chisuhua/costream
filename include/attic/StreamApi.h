#include "StreamType.h"


#define QUEUE_LOAD_API(op, order)                                           \
uint64_t QueueLoad##op##Index##order (const queue_t queue_handle);
QUEUE_LOAD_API(Read, Acquire)
QUEUE_LOAD_API(Read, Relaxed)
QUEUE_LOAD_API(Write, Acquire)
QUEUE_LOAD_API(Write, Relaxed)


#define QUEUE_STORE_API(op, order)                                              \
void QueueStore##op##Index##order (const queue_t queue_handle, uint64_t value);
QUEUE_STORE_API(Write, Relaxed)
QUEUE_STORE_API(Write, Release)
QUEUE_STORE_API(Read, Relaxed)
QUEUE_STORE_API(Read, Release)

