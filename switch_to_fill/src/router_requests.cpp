#include "router_requests.h"
#include "status.h"
#include <string>
#include "print_utils.h"
#include "offload.h"
#include "timer_utils.h"

int requests_completed = 0;

void yielding_router_request(fcontext_transfer_t arg){
   offload_request_args *args = (offload_request_args *)arg.data;
   struct completion_record * comp = args->comp;
   char *dst_payload = args->dst_payload;
   int id = args->id;
   std::string query = "/region/cluster/foo:key|#|etc";

   int status = submit_offload(comp, dst_payload);
   if(status == STATUS_FAIL){
     PRINT_ERR("Offload request failed\n");
     return;
   }
   fcontext_swap(arg.prev_context, NULL);

   uint32_t hashed = furc_hash((const char *)dst_payload, query.size(), 16);
   LOG_PRINT( LOG_DEBUG, "Hashing: %s %ld\n", dst_payload, query.size());

   requests_completed ++;
   fcontext_swap(arg.prev_context, NULL);
}

void yielding_router_request_stamp(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;

  struct completion_record * comp = args->comp;
  char *dst_payload = args->dst_payload;

  std::string query = "/region/cluster/foo:key|#|etc";

  ts0[id] = sampleCoderdtsc();

  int status = submit_offload(comp, dst_payload);
  if(status == STATUS_FAIL){
    PRINT_ERR("Offload request failed\n");
    return;
  }
  ts1[id] = sampleCoderdtsc();
  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();
  uint32_t hashed = furc_hash((const char *)dst_payload, query.size(), 16);
  ts3[id] = sampleCoderdtsc();
  LOG_PRINT( LOG_DEBUG, "Hashing: %s %ld\n", dst_payload, query.size());

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}


void yielding_ax_router_request_breakdown_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx){
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  timed_offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  string query = "/region/cluster/foo:key|#|etc";

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  requests_completed = 0;
  allocate_pre_deserialized_dsa_payloads(total_requests, &dst_bufs, query);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  off_args = (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);
  uint64_t *ts0, *ts1, *ts2, *ts3;
  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);

  for(int i=0; i<total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->comp = &comps[i];
    off_args[i]->dst_payload = dst_bufs[i];
    off_args[i]->id = i;
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  /* Pre-create the contexts */
  offload_req_xfer = (fcontext_transfer_t *)malloc(sizeof(fcontext_transfer_t) * total_requests);
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, yielding_router_request_stamp);

  execute_yielding_requests_closed_system_request_breakdown(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    offload_req_xfer, off_req_state, self,
    off_times, yield_to_resume_times, hash_times, idx);

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

  fcontext_destroy(self);
}



void blocking_router_request(fcontext_transfer_t arg){
   offload_request_args *args = (offload_request_args *)arg.data;
   struct completion_record * comp = args->comp;
   char *dst_payload = args->dst_payload;
   std::string query = "/region/cluster/foo:key|#|etc";

   int status = submit_offload(comp, dst_payload);
   if(status == STATUS_FAIL){
     return;
   }
   while(comp->status == COMP_STATUS_PENDING){
     _mm_pause();
   }
   LOG_PRINT( LOG_DEBUG, "Hashing: %s %ld\n", dst_payload, query.size());
   uint32_t hashed = furc_hash((const char *)dst_payload, query.size(), 16);

   requests_completed ++;
   fcontext_swap(arg.prev_context, NULL);
}

void blocking_router_request_stamp(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;

  struct completion_record * comp = args->comp;
  char *dst_payload = args->dst_payload;

  std::string query = "/region/cluster/foo:key|#|etc";

  ts0[id] = sampleCoderdtsc();

  int status = submit_offload(comp, dst_payload);
  if(status == STATUS_FAIL){
    return;
  }
  ts1[id] = sampleCoderdtsc();
  while(comp->status == COMP_STATUS_PENDING){
    _mm_pause();
  }
  ts2[id] = sampleCoderdtsc();
  LOG_PRINT( LOG_DEBUG, "Hashing: %s %ld\n", dst_payload, query.size());
  uint32_t hashed = furc_hash((const char *)dst_payload, query.size(), 16);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_router_request(fcontext_transfer_t arg){
  cpu_request_args *args = (cpu_request_args *)arg.data;
  router::RouterRequest *req = args->request;
  std::string *serialized = args->serialized;
  req->ParseFromString(*serialized);

  LOG_PRINT( LOG_DEBUG, "Hashing: %s\n", req->key().c_str());
  uint32_t hashed = furc_hash(req->key().c_str(), req->key().size(), 16);

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_router_request_stamp(fcontext_transfer_t arg){
  timed_cpu_request_args *args = (timed_cpu_request_args *)arg.data;
  router::RouterRequest *req = args->request;
  std::string *serialized = args->serialized;
  int id = args->id;

  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;

  ts0[id] = sampleCoderdtsc();
  req->ParseFromString(*serialized);
  const char *key = req->key().c_str();
  uint64_t size = req->key().size();
  ts1[id] = sampleCoderdtsc();

  LOG_PRINT( LOG_DEBUG, "Hashing: %s %ld\n", req->key().c_str(), size);
  uint32_t hashed = furc_hash(key, size, 16);
  ts2[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void serialize_request(router::RouterRequest *req, std::string *serialized){
  std::string query = "/region/cluster/foo:key|#|etc";
  std::string value = "bar";
  req->set_key(query);
  req->set_value(value);
  req->set_operation(0);
  req->SerializeToString(serialized);
}

void allocate_pre_deserialized_payloads(int total_requests, char ***p_dst_bufs, std::string query){
  *p_dst_bufs = (char **)malloc(sizeof(char *) * total_requests);
  for(int i=0; i<total_requests; i++){
    (*p_dst_bufs)[i] = (char *)malloc(sizeof(char) * query.size());
    memcpy((*p_dst_bufs)[i], query.c_str(), query.size());
  }
}

void allocate_pre_deserialized_dsa_payloads(int total_requests, char ***p_dst_bufs, std::string query){
  *p_dst_bufs = (char **)malloc(sizeof(char *) * total_requests);
  for(int i=0; i<total_requests; i++){
    (*p_dst_bufs)[i] = (char *)malloc(sizeof(char) * query.size());
    dsa_llc_realloc((*p_dst_bufs)[i], (void *)(query.c_str()), query.size());
  }
}


void blocking_ax_router_request_breakdown_test(
  int requests_sampling_interval, int total_requests,
  uint64_t *off_times, uint64_t *wait_times, uint64_t *hash_times, int idx){
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  timed_offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  std::string query = "/region/cluster/foo:key|#|etc";

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  uint64_t *ts0, *ts1, *ts2, *ts3; /* request args ts*/


  requests_completed = 0;
  allocate_pre_deserialized_dsa_payloads(total_requests, &dst_bufs, query);

  /* Pre-allocate the CRs */
  allocate_crs(total_requests, &comps);

  /* Pre-allocate the request args */
  off_args = (timed_offload_request_args **)malloc(sizeof(timed_offload_request_args *) * total_requests);
  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts3 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  for(int i=0; i<total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->comp = &comps[i];
    off_args[i]->dst_payload = dst_bufs[i];
    off_args[i]->id = i;
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
  }

  /* Pre-create the contexts */
  off_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);

  create_contexts(off_req_state, total_requests, blocking_router_request_stamp);

  execute_blocking_requests_closed_system_request_breakdown(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    comps, off_args,
    NULL, off_req_state, self,
    off_times, wait_times, hash_times, idx);

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
}


void cpu_router_request_breakdown(int requests_sampling_interval,
  int total_requests, uint64_t *deser_times, uint64_t *hash_times, int idx){
  using namespace std;
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  fcontext_state_t *self = fcontext_create_proxy();
  router::RouterRequest **deserializeIntoThisRequest;
  deserializeIntoThisRequest = (router::RouterRequest **)malloc(sizeof(router::RouterRequest *) * total_requests);
  std::string **serializedMCReqStrings = (string **)malloc(sizeof(string *) * total_requests);
  for(int i=0; i<total_requests; i++){
    deserializeIntoThisRequest[i] = new router::RouterRequest(); /*preallocated request obj*/
    serializedMCReqStrings[i] = new string();
    serialize_request(deserializeIntoThisRequest[i], serializedMCReqStrings[i]);
  }

  timed_cpu_request_args **cpu_args;
  uint64_t *ts0, *ts1, *ts2;
  ts0 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts1 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  ts2 = (uint64_t *)malloc(sizeof(uint64_t) * total_requests);
  cpu_args = (timed_cpu_request_args **)malloc(sizeof(timed_cpu_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    cpu_args[i] = (timed_cpu_request_args *)malloc(sizeof(timed_cpu_request_args));
    cpu_args[i]->request = deserializeIntoThisRequest[i];
    cpu_args[i]->serialized = serializedMCReqStrings[i];

    cpu_args[i]->ts0 = ts0;
    cpu_args[i]->ts1 = ts1;
    cpu_args[i]->ts2 = ts2;
    cpu_args[i]->id = i;
  }

  fcontext_state_t **cpu_req_state;
  cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
  create_contexts(cpu_req_state, total_requests, cpu_router_request_stamp);

  requests_completed = 0;
  execute_cpu_requests_closed_system_request_breakdown(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    NULL, cpu_args,
    cpu_req_state, self,
    deser_times, hash_times, idx);

  free_contexts(cpu_req_state, total_requests);
  for(int i=0; i<total_requests; i++){
    delete deserializeIntoThisRequest[i];
    delete serializedMCReqStrings[i];
    free(cpu_args[i]);
  }
  free(cpu_args);
  free(deserializeIntoThisRequest);
  free(serializedMCReqStrings);

  fcontext_destroy(self);

}

void cpu_router_closed_loop_test(int requests_sampling_interval, int total_requests, uint64_t *exetimes, int idx){
  using namespace std;
  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  fcontext_state_t *self = fcontext_create_proxy();
  router::RouterRequest **deserializeIntoThisRequest;
  deserializeIntoThisRequest = (router::RouterRequest **)malloc(sizeof(router::RouterRequest *) * total_requests);
  string **serializedMCReqStrings = (string **)malloc(sizeof(string *) * total_requests);
  for(int i=0; i<total_requests; i++){
    deserializeIntoThisRequest[i] = new router::RouterRequest(); /*preallocated request obj*/
    serializedMCReqStrings[i] = new string();
    serialize_request(deserializeIntoThisRequest[i], serializedMCReqStrings[i]);
  }

  cpu_request_args **cpu_args;
  cpu_args = (cpu_request_args **)malloc(sizeof(cpu_request_args *) * total_requests);
  for(int i=0; i<total_requests; i++){
    cpu_args[i] = (cpu_request_args *)malloc(sizeof(cpu_request_args));
    cpu_args[i]->request = deserializeIntoThisRequest[i];
    cpu_args[i]->serialized = serializedMCReqStrings[i];
  }

  fcontext_state_t **cpu_req_state;
  cpu_req_state = (fcontext_state_t **)malloc(sizeof(fcontext_state_t *) * total_requests);
  create_contexts(cpu_req_state, total_requests, cpu_router_request);

  requests_completed = 0;
  execute_cpu_requests_closed_system_with_sampling(
    requests_sampling_interval, total_requests,
    sampling_interval_completion_times, sampling_interval_timestamps,
    NULL, cpu_args,
    cpu_req_state, self,
    exetimes, idx);

  free_contexts(cpu_req_state, total_requests);
  for(int i=0; i<total_requests; i++){
    delete deserializeIntoThisRequest[i];
    delete serializedMCReqStrings[i];
    free(cpu_args[i]);
  }
  free(cpu_args);
  free(deserializeIntoThisRequest);
  free(serializedMCReqStrings);

  fcontext_destroy(self);

}

void blocking_ax_router_closed_loop_test(int requests_sampling_interval, int total_requests,
  uint64_t *exetime, int idx){
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  string query = "/region/cluster/foo:key|#|etc";

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];



  requests_completed = 0;
  allocate_pre_deserialized_dsa_payloads(total_requests, &dst_bufs, query);

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
    NULL, off_req_state, self, exetime, idx);

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
}

/*
  exetime needs to be (total_requests / requests_sampling_interval) x test_iterations

  the caller needs to ensure the "early yielder underutilization" problem does not
  impact the reported offered load.

  caller must tune the sampling interval and choose the exetime samples that are not impacted
  by underutilization

  indexing into the array: caller must ensure they increment the start_idx by
    (total_requests / requests_sampling_interval)
*/
void yielding_ax_router_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *exetime, int start_idx){
  using namespace std;
  fcontext_state_t *self = fcontext_create_proxy();
  char**dst_bufs;
  ax_comp *comps;
  offload_request_args **off_args;
  fcontext_transfer_t *offload_req_xfer;
  fcontext_state_t **off_req_state;
  string query = "/region/cluster/foo:key|#|etc";

  int sampling_intervals = (total_requests / requests_sampling_interval);
  int sampling_interval_timestamps = sampling_intervals + 1;
  uint64_t sampling_interval_completion_times[sampling_interval_timestamps];

  requests_completed = 0;
  allocate_pre_deserialized_dsa_payloads(total_requests, &dst_bufs, query);

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
    offload_req_xfer, off_req_state, self,
    exetime, start_idx);


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

  fcontext_destroy(self);
}