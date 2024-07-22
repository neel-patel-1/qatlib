#include "print_utils.h"
#include "emul_ax.h"
extern "C"{
#include "fcontext.h"
}
#include "offload.h"
#include "router_requests.h"
#include <string>
#include "status.h"
#include "timer_utils.h"

bool gDebugParam = false;
uint64_t *ts0, *ts1, *ts2, *ts3, *ts4;

int main(){
  gDebugParam = true;
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 511;
  start_non_blocking_ax(&ax_td, &ax_running, offload_time, 10);

  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;

  int samples = 10;

  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * samples);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * samples);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * samples);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * samples);
  ts4 = (uint64_t *)malloc(sizeof(uint64_t) * samples);

  std::string query = "/region/cluster/foo:key|#|etc";

  requests_completed = 0;
  offload_request_args *arg = (offload_request_args *)malloc(sizeof(offload_request_args));
  arg->comp = (ax_comp *)malloc(sizeof(ax_comp));
  arg->comp->status = COMP_STATUS_PENDING;
  arg->dst_payload = (char *)malloc(query.size());
  arg->id = 0;

  for(int i=0; i<samples; i++){
    ts0[i] = sampleCoderdtsc();

    offload_request_args *args = (offload_request_args *)arg;
    struct completion_record * comp = args->comp;
    char *dst_payload = args->dst_payload;

    ts1[i] = sampleCoderdtsc();

    int status = submit_offload(comp, dst_payload);
    if(status == STATUS_FAIL){
      return STATUS_FAIL;
    }

    ts2[i] = sampleCoderdtsc();
    while(comp->status == COMP_STATUS_PENDING){
      _mm_pause();
    }

    ts3[i] = sampleCoderdtsc();
    furc_hash((const char *)dst_payload, query.size(), 16);
    ts4[i] = sampleCoderdtsc();

    requests_completed ++;
  }

  uint64_t avg, diff_array[samples];
  avg_samples_from_arrays(diff_array, avg, ts1, ts0, samples);
  PRINT_DBG("Unpacking_args: %lu\n", avg);
  avg_samples_from_arrays(diff_array, avg, ts2, ts1, samples);
  PRINT_DBG("Submit_offload: %lu\n", avg);
  avg_samples_from_arrays(diff_array, avg, ts3, ts2, samples);
  PRINT_DBG("Wait_for_completion: %lu\n", avg);
  avg_samples_from_arrays(diff_array, avg, ts4, ts3, samples);
  PRINT_DBG("Hashing: %lu\n", avg);

  stop_non_blocking_ax(&ax_td, &ax_running);

  /* execute timestamped blocking and yielding router request */

}