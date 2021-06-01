STREAM_API(signal, create)
STREAM_API(signal, destroy)
STREAM_API(signal, load_scacquire)
STREAM_API(signal, load_relaxed)
STREAM_API(signal, store_relaxed)
STREAM_API(signal, store_screlease)
STREAM_API(signal, or_relaxed)

STREAM_API(signal, wait_scacquire)
STREAM_API(signal, wait_relaxed)

STREAM_API(signal, group_create)
STREAM_API(signal, group_destroy)

STREAM_API(signal, group_wait_any_scacquire)

STREAM_API(signal, get_start_ts)

STREAM_API(queue, create)
STREAM_API(queue, destroy)
STREAM_API(queue, inactive)

