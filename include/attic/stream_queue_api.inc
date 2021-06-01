QUEUE_LOAD_API(rptr, scacqurie)
QUEUE_LOAD_API(rptr, relaxed)
QUEUE_LOAD_API(wptr, scacqurie)
QUEUE_LOAD_API(wptr, relaxed)

QUEUE_STORE_API(wptr, relaxed)
QUEUE_STORE_API(wptr, screlease)
QUEUE_STORE_API(rptr, relaxed)
QUEUE_STORE_API(rptr, screlease)

QUEUE_OP_API(cas, scacq_screl)
QUEUE_OP_API(cas, scacquire)
QUEUE_OP_API(cas, relaxed)
QUEUE_OP_API(cas, screlease)

QUEUE_OP_API(add, scacq_screl)
QUEUE_OP_API(add, scacquire)
QUEUE_OP_API(add, relaxed)
QUEUE_OP_API(add, screlease)
