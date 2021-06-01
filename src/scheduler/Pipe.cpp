#include "Pipe.h"
#include "processor/Processor.h"
#include "queue.h"
#include "signal.h"
#include "stream.h"
#include <unistd.h>

bool Pipe::QueueEnable(uint32_t queue_id) {
    reg_CP_QUEUE_CTRL data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_CTRL), data.val);
    return data.bits.enable;
}

void* Pipe::QueueRingBase(uint32_t queue_id) {
    reg_CP_QUEUE_RB_BASE data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_BASE_HI), data.bits.hi);
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_BASE_LO), data.bits.lo);
    return (void*)data.val;
}

uint32_t Pipe::QueueRingSize(uint32_t queue_id) {
    reg_CP_QUEUE_RB_SIZE data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_SIZE), data.val);
    return data.val;
}

void* Pipe::QueueRingRPTR(uint32_t queue_id) {
    reg_CP_QUEUE_RB_RPTR data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_RPTR_HI), data.bits.hi);
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_RPTR_LO), data.bits.lo);
    return (void*)data.val;
}

void* Pipe::QueueRingWPTR(uint32_t queue_id) {
    reg_CP_QUEUE_RB_WPTR data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_WPTR_HI), data.bits.hi);
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_RB_WPTR_LO), data.bits.lo);
    return (void*)data.val;
}

void* Pipe::QueueDoorBellBase(uint32_t queue_id) {
    reg_CP_QUEUE_DOORBELL_BASE data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_DOORBELL_BASE_HI), data.bits.hi);
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_DOORBELL_BASE_LO), data.bits.lo);
    return (void*)data.val;
}

uint32_t Pipe::QueueDoorBellOffset(uint32_t queue_id) {
    reg_CP_QUEUE_DOORBELL_OFFSET data;
    sp_->ReadRegFunc(mmCP_QUEUE_REG_ADDR(queue_id, CP_QUEUE_DOORBELL_OFFSET), data.val);
    return data.val;
}

void Pipe::ConnectDownQueue(uint32_t queue_id) {
    while(!QueueEnable()) {};
    auto down_info = make_shared<DownQueueInfo>();
    down_info->base_address = QueueRingBase(queue_id);
    down_info->base_size = QueueRingSize(queue_id);
    down_info->write_ptr = QueueRingWPTR(queue_id);
    down_info->read_ptr = QueueRingRPTR(queue_id);
    down_info->doorbell_base = QueueDoorBellBase(queue_id);
    down_info->doorbell_offset = QueueDoorBellOffset(queue_id);
    down_queues_.insert(std::make_pair(queue_id, down_info));
}

void Pipe::MakeUpQueue() {
    auto up_info = make_shared<UpQueueInfo>();
    sp_->CreateQueue(up_info);
    up_queues_.push_back(up_info);
}

void Pipe::Execute() {
    // MakeQueue(queue_id_);
    uint32_t up_queue_id = 0;
        bool queue_busy = false;
        for (auto& down_queue : down_queues_) {
            core::AqlPacket* buffer = static_cast<core::AqlPacket*>(down_queue.second->base_address);
            // hsa_signal_t signal = cp_queue.hsa_queue.doorbell_signal;
            uint32_t size = down_queue.second->base_size;

            down_queue.second->write_dispatch_id = *(uint64_t*)(down_queue.second->write_ptr);

            uint64_t read = down_queue.second->read_dispatch_id;
            while (read != down_queue.second->write_dispatch_id) {
                core::AqlPacket& pkt = buffer[read % size];
                const uint8_t packet_type = pkt.dispatch.header >> HSA_PACKET_HEADER_TYPE;

                if (packet_type == HSA_PACKET_TYPE_INVALID) {
                    printf("ERROR: receive invalid packet int cp");
                    break;
                } else if (packet_type == HSA_PACKET_TYPE_KERNEL_DISPATCH) {
                    hsa_kernel_dispatch_packet_t& dispatch_pkt = pkt.dispatch;
    		        // sp_->LaunchGrid(dispatch_pkt, up_queues_[up_queue_id]);
                } else if (packet_type == HSA_PACKET_TYPE_DMA_COPY) {
                    hsa_dma_copy_packet_t& dma_pkt = pkt.dma_copy;
    			    // sp_->LaunchDMA(dma_pkt, up_queues_[up_queue_id]);
                } else if (packet_type == HSA_PACKET_TYPE_TASK) {
                    hsa_task_packet_t& task_pkt = pkt.task;
                    sp_->AddTask((Task*)task_pkt.task)
    			    // sp_->LaunchDMA(dma_pkt, up_queues_[up_queue_id]);
                } else if (packet_type == HSA_PACKET_TYPE_BARRIER_AND ||
                    packet_type == HSA_PACKET_TYPE_BARRIER_OR) {
                    if (pkt.barrier_and.completion_signal.handle != 0) {
                        core::SharedSignal* sig = ::core::SharedSignal::Object(pkt.barrier_and.completion_signal);
                        uint64_t signal_value = sig->amd_signal.value - 1;
                        sig->amd_signal.value = signal_value;
                    }

                }
                read++;
            	auto header=pkt.dispatch.header;
    	        header &= 0xFF00;
                header |= (HSA_PACKET_TYPE_INVALID << HSA_PACKET_HEADER_TYPE);
    	        *(volatile uint16_t*)&pkt.dispatch.header=header;

    	        queue_busy = true;
            }
            if (read != down_queue.second->read_dispatch_id) {
                // update read pointer
                down_queue.second->read_dispatch_id = read;
                // update host read_dispatch_id
                *(uint64_t*)(down_queue.second->read_ptr)  = down_queue.second->read_dispatch_id;
            }
        }

        //if (not queue_busy) sleep(1);
}

