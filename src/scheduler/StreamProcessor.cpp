#include "StreamProcessor.h"
// #include "Pipe.h"
#include "core/IQueue.h"
#include "core/ISignal.h"
#include "Stream.h"
#include <unistd.h>
#include <list>

using namespace std;

void StreamProcessor::LaunchGrid(kernel_dispatch_packet_t& aql_pkt, shared_ptr<UpQueueInfo> up_queue) {
    uint32_t GridDimX = aql_pkt.grid_size_x;
    uint32_t GridDimY = aql_pkt.grid_size_y;
    uint32_t GridDimZ = aql_pkt.grid_size_z;

    bool need_resp = false;
    if (aql_pkt.completion_signal.handle != 0)
        need_resp = true;

    // auto msg_list = make_shared<std::list<shared_ptr<Message>>>();
    for (uint32_t blockIdZ = 0; blockIdZ < GridDimZ; blockIdZ++) {
        for (uint32_t blockIdY = 0; blockIdY < GridDimY; blockIdY++) {
            for (uint32_t blockIdX = 0;  blockIdX < GridDimX; blockIdX++) {
                /*
                auto pkt = make_shared<CuDispatchPacket>();
                pkt->disp_info.kernel_addr = aql_pkt.kernel_object;
                pkt->disp_info.kernel_args = aql_pkt.kernarg_address;
                pkt->disp_info.kernel_ctrl.val = aql_pkt.kernel_ctrl;
                pkt->disp_info.kernel_mode.val = aql_pkt.kernel_mode;
                // pkt->disp_info.kernel_resource.val = aql_pkt.kernel_resource;
                pkt->disp_info.gridDimX = aql_pkt.grid_size_x;
                pkt->disp_info.gridDimY = aql_pkt.grid_size_y;
                pkt->disp_info.gridDimZ = aql_pkt.grid_size_z;
                pkt->disp_info.blockDimX = aql_pkt.workgroup_size_x;
                pkt->disp_info.blockDimY = aql_pkt.workgroup_size_y;
                pkt->disp_info.blockDimZ = aql_pkt.workgroup_size_z;
                pkt->disp_info.blockIdX = blockIdX;
                pkt->disp_info.blockIdY = blockIdY;
                pkt->disp_info.blockIdZ = blockIdZ;
                */
                /*
                auto msg = make_shared<CuReqMessage>(pkt);
                if (need_resp) {
                    m_response_queue->Push(msg);
                    msg_list->push_back(static_pointer_cast<Message>(msg));
                }
                PushReq("cu0", msg);
                */
            }
        }
    }
/*
    if (need_resp)
        m_dispatch_response_list.insert(make_pair(aql_pkt.completion_signal.handle, msg_list));
        */
}


void StreamProcessor::LaunchDMA(dma_copy_packet_t& aql_pkt, shared_ptr<UpQueueInfo> up_queue) {
    /*
    auto pkt = make_shared<DmaCopyPacket>();
    pkt->src = aql_pkt.src;
    pkt->dst = aql_pkt.dst;
    pkt->length = aql_pkt.bytes;

    bool need_resp = false;
    if (aql_pkt.completion_signal.handle != 0)
        need_resp = true;

    auto msg_list = make_shared<std::list<shared_ptr<Message>>>();
    auto msg = make_shared<CeReqMessage>(pkt);
    if (need_resp) {
        m_response_queue->Push(static_pointer_cast<Message>(msg));
        msg_list->push_back(static_pointer_cast<Message>(msg));
    }
    PushReq("ce", msg);

    if (need_resp) {
        m_dispatch_response_list.insert(make_pair(aql_pkt.completion_signal.handle, msg_list));
    }
    */
}

void StreamProcessor::ProcessDispatchDone()  {
/*
    for (auto it = m_dispatch_response_list.begin(); it != m_dispatch_response_list.end();) {
        hsa_signal_t completion_signal;
        completion_signal.handle = it->first;
        //hsa_signal_t completion_signal = it->first;
        //uint64_t completion_signal = it->first;
        shared_ptr<std::list<shared_ptr<Message>>> msg_list = it->second;

        bool done = true;
        for (auto &msg_it : *msg_list) {
            if (!msg_it->IsRespDone()) {
                done = false;
                break;
            }
        }
        if (done) {
            core::SharedSignal* sig = ::core::SharedSignal::Object(completion_signal);
            uint64_t signal_value = sig->amd_signal.value - 1;
            sig->amd_signal.value = signal_value;
            it = m_dispatch_response_list.erase(it);

        } else {
            it++;
        }

    }
    */
}

bool StreamProcessor::Boot(size_t scratch_base, size_t scratch_size, size_t size_per_queue, ReadRegFType read_reg_func) {
    auto sp = make_shared<StreamProcessor>(scratch_base, scratch_size, size_per_queue);
    sp->ReadRegFunc = read_reg_func;
    while(!sp->Stopped()) {
        sp->Execute();
    }
    for (auto& t : sp->active_pipes_) {
        t.join();
    }
}

StreamProcessor::StreamProcessor(size_t scratch_base, size_t scratch_size, size_t size_per_queue)
           : scratch_base_(scratch_base)
           , scratch_size_(scratch_size)
           , scratch_size_per_queue_(size_per_queue)
{
    pipes_.resize(CP_MAX_QUEUE_NUM);
    active_pipes_.resize(CP_MAX_QUEUE_NUM);
    for (int i = 0; i < CP_MAX_QUEUE_NUM; i++) {
       pipes_[i] = make_shared<Pipe>(this, i);
    }

    scratch_pool_. ~SmallHeap();
    new (&scratch_pool_) SmallHeap((void*)scratch_base_, scratch_size_);
};

StreamProcessor::~StreamProcessor() {
}

void StreamProcessor::CreateQueue(shared_ptr<UpQueueInfo> qinfo) {
    std::unique_lock<std::mutex> lock(scratch_lock_);
    qinfo->base_address = scratch_pool_.alloc(scratch_size_per_queue_);
    assert(qinfo->base_address != nullptr);
}

void StreamProcessor::ReleaseQueue(shared_ptr<UpQueueInfo> qinfo) {
    std::unique_lock<std::mutex> lock(scratch_lock_);
    scratch_pool_.free(qinfo->base_address);
}

// LaunchGrid(root_function_, (CmdDispatchPacket*)&pkt, wave_size_, main_co_);
void StreamProcessor::Execute() {
    all_stopped_ = true;
    for (uint32_t i = 0; i <CP_MAX_QUEUE_NUM; i++) {
        bool stopped = false;
        if (not pipes_[i]->IsActive()) {
            if (pipes_[i]->QueueEnable(i)) {
                active_pipes_.push_back(std::thread(&Pipe::Execute, pipes_[i]));
                pipes_[i]->SetActive(true);
            }
        } else if (pipes_[i]->IsStopped()) {
            stopped = true;
        }
        all_stopped_ &= stopped;
    }
};


