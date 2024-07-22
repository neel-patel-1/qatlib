#include "print_utils.h"
#include "emul_ax.h"
extern "C"{
#include "fcontext.h"
}
#include "offload.h"
#include "router_requests.h"
#include <string>
#include "status.h"

bool gDebugParam = false;
uint64_t *ts0, *ts1;

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

  int total_requests = 10000;

  std::string query = "/region/cluster/foo:key|#|etc";

  offload_request_args *arg = (offload_request_args *)malloc(sizeof(offload_request_args));
  arg->comp = (ax_comp *)malloc(sizeof(ax_comp));
  arg->comp->status = COMP_STATUS_PENDING;
  arg->dst_payload = (char *)malloc(query.size());


  offload_request_args *args = (offload_request_args *)arg;
  struct completion_record * comp = args->comp;
  char *dst_payload = args->dst_payload;

  int status = submit_offload(comp, dst_payload);
  if(status == STATUS_FAIL){
    return STATUS_FAIL;
  }
  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }
  furc_hash((const char *)dst_payload, query.size(), 16);

  requests_completed ++;


  /* execute timestamped blocking and yielding router request */

}