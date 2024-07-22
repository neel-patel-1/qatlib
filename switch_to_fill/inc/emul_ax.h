#ifndef EMUL_AX_H
#define EMUL_AX_H
#include <stdint.h>
#include <atomic>
#include <cstddef>

#define SUBMIT_SUCCESS 0
#define SUBMIT_FAIL 1
#define OFFLOAD_RECEIVED 1
#define OFFLOAD_REQUESTED 0
#define COMP_STATUS_COMPLETED 1
#define COMP_STATUS_PENDING 0

extern std::atomic<int> submit_flag;
extern std::atomic<int> submit_status;
extern std::atomic<uint64_t> compl_addr;
extern std::atomic<uint64_t> p_dst_buf;

typedef struct completion_record{
  int status;
} ax_comp;

class offload_entry{
  public:
  offload_entry(){
    start_time = 0;
    comp_time = 0;
    comp = NULL;
  }
  offload_entry(uint64_t start, uint64_t compl_time, ax_comp *c){
    start_time = start;
    comp_time = compl_time;
    comp = c;
  }
  uint64_t start_time;
  uint64_t comp_time;
  int32_t id;
  ax_comp *comp;
};

typedef struct _ax_params {
  int max_inflights;
  uint64_t offload_time;
  bool *ax_running;
} ax_params;


#endif