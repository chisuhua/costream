#pragma once
#include "stream.h"
#include "AddrDef.h"
#include "CpReg.h"
// #include "Pipe.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <thread>
#include "util/small_heap.h"

using namespace std;

class Pipe;

struct DownQueueInfo {
    void *base_address;
    uint32_t base_size;
    void *write_ptr;
    void *read_ptr;
    void *doorbell_base;
    uint32_t doorbell_offset;
    uint32_t write_dispatch_id;
    uint32_t read_dispatch_id;
};

struct UpQueueInfo {
    void *base_address;
    uint32_t base_size;
    void *write_ptr;
    void *read_ptr;
};


typedef void (*ReadRegFType)(uint64_t reg, uint32_t &data);

class StreamProcessor {
    public:
        StreamProcessor(size_t scratch_base, size_t scratch_size, size_t size_per_queue);

        virtual ~StreamProcessor();

        static bool Boot(size_t scratch_base, size_t scratch_size, size_t size_per_queue, ReadRegFType);

        void CreateQueue(shared_ptr<UpQueueInfo> qinfo);

        void ReleaseQueue(shared_ptr<UpQueueInfo> qinfo);

        virtual void Execute();
        bool Stopped() { return all_stopped_;};
        void Quit();

    public:
        void LaunchGrid(kernel_dispatch_packet_t& aql_pkt, shared_ptr<UpQueueInfo>);
        void LaunchDMA(dma_copy_packet_t& aql_pkt, shared_ptr<UpQueueInfo>) ;
        void ProcessDispatchDone();
        // void ProcessRespMsg(MsgPtr);
        void TranslateVA(uint64_t va, uint64_t *pa, bool emu_addr = true) ;

        ReadRegFType ReadRegFunc;

    private:
        SmallHeap scratch_pool_;
        size_t scratch_base_;
        size_t scratch_size_;
        size_t scratch_size_per_queue_;
        std::mutex scratch_lock_;
        std::vector<shared_ptr<Pipe>> pipes_;
        std::vector<std::thread> active_pipes_;


        uint32_t dispatch_id = 0;
        uint32_t m_max_dispatch_num = 8;
        bool all_stopped_;
};
