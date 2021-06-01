#pragma once
#include <stdint.h>

#include <string>

#include "util/os.h"
#include "util/utils.h"


class Flag {
 public:
  enum SDMA_OVERRIDE { SDMA_DISABLE, SDMA_ENABLE, SDMA_DEFAULT };

  explicit Flag() { Refresh(); }

  virtual ~Flag() {}

  void Refresh() {
    std::string var = os::GetEnvVar("HSA_CHECK_FLAT_SCRATCH");
    check_flat_scratch_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_ENABLE_VM_FAULT_MESSAGE");
    enable_vm_fault_message_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_QUEUE_FAULT_MESSAGE");
    enable_queue_fault_message_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_INTERRUPT");
    enable_interrupt_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_ENABLE_SDMA");
    enable_sdma_ = (var == "0") ? SDMA_DISABLE : ((var == "1") ? SDMA_ENABLE : SDMA_DEFAULT);

    visible_gpus_ = os::GetEnvVar("ROCR_VISIBLE_DEVICES");
    filter_visible_gpus_ = os::IsEnvVarSet("ROCR_VISIBLE_DEVICES");

    var = os::GetEnvVar("HSA_RUNNING_UNDER_VALGRIND");
    running_valgrind_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_SDMA_WAIT_IDLE");
    sdma_wait_idle_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_MAX_QUEUES");
    max_queues_ = static_cast<uint32_t>(atoi(var.c_str()));

    var = os::GetEnvVar("HSA_SCRATCH_MEM");
    scratch_mem_size_ = atoi(var.c_str());

    tools_lib_names_ = os::GetEnvVar("HSA_TOOLS_LIB");

    var = os::GetEnvVar("HSA_TOOLS_REPORT_LOAD_FAILURE");

    ifdebug {
      report_tool_load_failures_ = (var == "1") ? true : false;
    } else {
      report_tool_load_failures_ = (var == "0") ? false : true;
    }

    var = os::GetEnvVar("HSA_DISABLE_FRAGMENT_ALLOCATOR");
    disable_fragment_alloc_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_ENABLE_SDMA_HDP_FLUSH");
    enable_sdma_hdp_flush_ = (var == "0") ? false : true;

    var = os::GetEnvVar("HSA_REV_COPY_DIR");
    rev_copy_dir_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_FORCE_FINE_GRAIN_PCIE");
    fine_grain_pcie_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_NO_SCRATCH_RECLAIM");
    no_scratch_reclaim_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_NO_SCRATCH_THREAD_LIMITER");
    no_scratch_thread_limit_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_DISABLE_IMAGE");
    disable_image_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_LOADER_ENABLE_MMAP_URI");
    loader_enable_mmap_uri_ = (var == "1") ? true : false;

    var = os::GetEnvVar("HSA_FORCE_SDMA_SIZE");
    force_sdma_size_ = var.empty() ? 1024 * 1024 : atoi(var.c_str());

    var = os::GetEnvVar("HSA_IGNORE_SRAMECC_MISREPORT");
    check_sramecc_validity_ = (var == "1") ? false : true;
  }

  bool check_flat_scratch() const { return check_flat_scratch_; }

  bool enable_vm_fault_message() const { return enable_vm_fault_message_; }

  bool enable_queue_fault_message() const { return enable_queue_fault_message_; }

  bool enable_interrupt() const { return enable_interrupt_; }

  bool enable_sdma_hdp_flush() const { return enable_sdma_hdp_flush_; }

  bool running_valgrind() const { return running_valgrind_; }

  bool sdma_wait_idle() const { return sdma_wait_idle_; }

  bool report_tool_load_failures() const { return report_tool_load_failures_; }

  bool disable_fragment_alloc() const { return disable_fragment_alloc_; }

  bool rev_copy_dir() const { return rev_copy_dir_; }

  bool fine_grain_pcie() const { return fine_grain_pcie_; }

  bool no_scratch_reclaim() const { return no_scratch_reclaim_; }

  bool no_scratch_thread_limiter() const { return no_scratch_thread_limit_; }

  SDMA_OVERRIDE enable_sdma() const { return enable_sdma_; }

  std::string visible_gpus() const { return visible_gpus_; }

  bool filter_visible_gpus() const { return filter_visible_gpus_; }

  uint32_t max_queues() const { return max_queues_; }

  size_t scratch_mem_size() const { return scratch_mem_size_; }

  std::string tools_lib_names() const { return tools_lib_names_; }

  bool disable_image() const { return disable_image_; }

  bool loader_enable_mmap_uri() const { return loader_enable_mmap_uri_; }

  size_t force_sdma_size() const { return force_sdma_size_; }

  bool check_sramecc_validity() const { return check_sramecc_validity_; }

 private:
  bool check_flat_scratch_;
  bool enable_vm_fault_message_;
  bool enable_interrupt_;
  bool enable_sdma_hdp_flush_;
  bool running_valgrind_;
  bool sdma_wait_idle_;
  bool enable_queue_fault_message_;
  bool report_tool_load_failures_;
  bool disable_fragment_alloc_;
  bool rev_copy_dir_;
  bool fine_grain_pcie_;
  bool no_scratch_reclaim_;
  bool no_scratch_thread_limit_;
  bool disable_image_;
  bool loader_enable_mmap_uri_;
  bool check_sramecc_validity_;

  SDMA_OVERRIDE enable_sdma_;

  bool filter_visible_gpus_;
  std::string visible_gpus_;

  uint32_t max_queues_;

  size_t scratch_mem_size_;

  std::string tools_lib_names_;

  size_t force_sdma_size_;

  DISALLOW_COPY_AND_ASSIGN(Flag);
};


