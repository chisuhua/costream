#pragma once

class EventPool {
   public:
    struct Deleter {
      void operator()(HsaEvent* evt) { DestroyEvent(evt); }
    };
    using unique_event_ptr = ::std::unique_ptr<HsaEvent, Deleter>;

    EventPool() : allEventsAllocated(false) {}

    HsaEvent* alloc();
    void free(HsaEvent* evt);
    void clear() {
      events_.clear();
      allEventsAllocated = false;
    }

   private:
    KernelMutex lock_;
    std::vector<unique_event_ptr> events_;
    bool allEventsAllocated;
  };

  HsaEvent* CreateEvent(HSA_EVENTTYPE type, bool manual_reset);
  void DestroyEvent(HsaEvent* evt);

  EventPool* GetPool() {
      static EventPool *pool {nullptr};
      if (!pool) {
          pool = new EventPool();
      }
      return pool;
  }

}
