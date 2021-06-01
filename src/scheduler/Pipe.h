#pragma once
#include "stream.h"
#include "AddrDef.h"
#include "CpReg.h"
#include <vector>
#include <map>
#include <mutex>
#include <memory>

using namespace std;

class Processor;
struct DownQueueInfo;
struct UpQueueInfo;


class Pipe {
    public:
        Pipe(Processor* sp, uint32_t queue_id)
            : sp_(sp)
            , queue_id_(queue_id)
        {};

        bool QueueEnable(uint32_t queue_id);

        void* QueueRingBase(uint32_t queue_id);

        uint32_t QueueRingSize(uint32_t queue_id);

        void* QueueRingRPTR(uint32_t queue_id);

        void* QueueRingWPTR(uint32_t queue_id);

        void* QueueDoorBellBase(uint32_t queue_id);

        uint32_t QueueDoorBellOffset(uint32_t queue_id);

        ~Pipe() {};

        void Destroy() {};
        void Execute() ;
        void ConnectDownQueue(uint32_t queue_id);
        void MakeUpQueue();
        bool IsStopped() { return stopped_; }
        bool IsActive() { return active_; }
        void SetActive(bool active) {
            active_ = active;
        }

    public:
        Processor *sp_;
        uint32_t queue_id_;

        std::map<uint32_t, shared_ptr<DownQueueInfo>> down_queues_;
        std::vector<std::shared_ptr<UpQueueInfo>> up_queues_;
        bool stopped_ {false};
        bool active_ {false};
};

