#include "timer_utils.h"
#include <pthread.h>
#include "print_utils.h"
#include "ch3_hash.h"
#include "status.h"
#include "router.pb.h"
#include "emul_ax.h"
#include "thread_utils.h"
#include "offload.h"

extern "C"{
#include "fcontext.h"
}
#include <cstdlib>
#include <atomic>
#include <list>
#include <cstring>
#include <x86intrin.h>
#include <string>

#include "router_requests.h"
#include "request_executors.h"
#include "context_management.h"
#include "rps.h"

/*
  End-to-End Evaluation
  - Blocking vs. Switch to Fill
  - Accelerator Data access overhead

  Accelerators
  - Deser
  - Pointer-chasing
  - MLP followed by (user,item) ranking

  Output generation/post-processing:
  - preallocated buffers: as long as we know where to access them for post-processing

  Switch-To-Fill Scheduling:
  - Count completed requests to know when to terminate
*/


using namespace std;

bool gDebugParam = false;

int main(){
  gDebugParam = true;
  pthread_t ax_td;
  bool ax_running = true;
  int offload_time = 511;
  start_non_blocking_ax(&ax_td, &ax_running, offload_time, 10);

  int requests_sampling_interval = 1000, total_requests = 10000;
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];
  int sampling_interval = 0;

  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;


  string query = "/region/cluster/foo:key|#|etc";
  string value = "bar";

    self = fcontext_create_proxy();

    /* switch to fill router requests */
    /*reset offload counter*/
    requests_sampling_interval = 1000, total_requests = 10000;
    sampling_intervals = (total_requests / requests_sampling_interval);
    sampling_interval_timestamps = sampling_intervals + 1;

    requests_completed = 0;

    /* Pre-allocate the payloads */
    allocate_pre_deserialized_payloads(total_requests, &dst_bufs, query);

    /* Pre-allocate the CRs */
    allocate_crs(total_requests, &comps);

    /* Pre-allocate the request args */
    allocate_offload_requests(total_requests, &off_args, comps, dst_bufs);

    /* Pre-create the contexts */
    offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
    off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

    create_contexts(off_req_state, total_requests, yielding_router_request);

    execute_yielding_requests_closed_system_with_sampling(
      requests_sampling_interval, total_requests,
      sampling_interval_completion_times, sampling_interval_timestamps,
      comps, off_args,
      offload_req_xfer, off_req_state, self);

    calculate_rps_from_samples(
      sampling_interval_completion_times,
      sampling_intervals,
      requests_sampling_interval,
      2100000000);

    /* teardown */
    free_contexts(off_req_state, total_requests);
    free(offload_req_xfer);
    free(comps);
    for(int i=0; i<total_requests; i++){
      free(off_args[i]);
      free(dst_bufs[i]);
    }
    free(off_args);
    free(dst_bufs);

    /*
      Pre-allocate all serialized requests used by the cpu requests

      execute_cpu_requests_closed_system_with_sampling
    */
    router::RouterRequest **serializedMCReqs;
    serializedMCReqs = (router::RouterRequest **)malloc(sizeof(router::RouterRequest *) * total_requests);
    string **serializedMCReqStrings = (string **)malloc(sizeof(string *) * total_requests);
    for(int i=0; i<total_requests; i++){
      serializedMCReqs[i] = new router::RouterRequest(); /*preallocated request obj*/
      serializedMCReqStrings[i] = new string();
      serialize_request(serializedMCReqs[i], serializedMCReqStrings[i]);
      // PRINT_DBG("Serialized: %s\n", serializedMCReqStrings[i]->c_str());
    }

    cpu_request_args **cpu_args;
    cpu_args = (cpu_request_args **)malloc(sizeof(cpu_request_args *) * total_requests);
    for(int i=0; i<total_requests; i++){
      cpu_args[i] = (cpu_request_args *)malloc(sizeof(cpu_request_args));
      cpu_args[i]->request = serializedMCReqs[i];
      cpu_args[i]->serialized = serializedMCReqStrings[i];
    }

    fcontext_state_t **cpu_req_state;
    cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
    create_contexts(cpu_req_state, total_requests, cpu_router_request);

    requests_sampling_interval = 10000, total_requests = 10000;
    sampling_intervals = (total_requests / requests_sampling_interval);
    sampling_interval_timestamps = sampling_intervals + 1;
    requests_completed = 0;

    execute_cpu_requests_closed_system_with_sampling(
      requests_sampling_interval, total_requests,
      sampling_interval_completion_times, sampling_interval_timestamps,
      comps, cpu_args,
      cpu_req_state, self);

    calculate_rps_from_samples(
      sampling_interval_completion_times,
      sampling_intervals,
      requests_sampling_interval,
      2100000000);

    free_contexts(cpu_req_state, total_requests);
    for(int i=0; i<total_requests; i++){
      delete serializedMCReqs[i];
      delete serializedMCReqStrings[i];
      free(cpu_args[i]);
    }
    free(cpu_args);
    free(serializedMCReqs);
    free(serializedMCReqStrings);


    requests_sampling_interval = 10000, total_requests = 10000;
    sampling_intervals = (total_requests / requests_sampling_interval);
    sampling_interval_timestamps = sampling_intervals + 1;

    requests_completed = 0;
    allocate_pre_deserialized_payloads(total_requests, &dst_bufs, query);

    /* Pre-allocate the CRs */
    allocate_crs(total_requests, &comps);

    /* Pre-allocate the request args */
    allocate_offload_requests(total_requests, &off_args, comps, dst_bufs);

    /* Pre-create the contexts */
    off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

    create_contexts(off_req_state, total_requests, blocking_router_request);

    execute_blocking_requests_closed_system_with_sampling(
      requests_sampling_interval, total_requests,
      sampling_interval_completion_times, sampling_interval_timestamps,
      comps, off_args,
      NULL, off_req_state, self);

    calculate_rps_from_samples(
      sampling_interval_completion_times,
      sampling_intervals,
      requests_sampling_interval,
      2100000000);

    /* teardown */
    free_contexts(off_req_state, total_requests);
    free(comps);
    for(int i=0; i<total_requests; i++){
      free(off_args[i]);
      free(dst_bufs[i]);
    }
    free(off_args);
    free(dst_bufs);




    fcontext_destroy(self);


  stop_non_blocking_ax(&ax_td, &ax_running);


  return 0;
}