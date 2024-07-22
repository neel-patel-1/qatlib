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

   furc_hash((const char *)dst_payload, query.size(), 16);

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
  furc_hash((const char *)dst_payload, query.size(), 16);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
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
   furc_hash((const char *)dst_payload, query.size(), 16);

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
  // PRINT_DBG("Hashing: %s %ld\n", dst_payload, query.size());
  furc_hash((const char *)dst_payload, query.size(), 16);
  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void cpu_router_request(fcontext_transfer_t arg){
  cpu_request_args *args = (cpu_request_args *)arg.data;
  router::RouterRequest *req = args->request;
  std::string *serialized = args->serialized;
  req->ParseFromString(*serialized);

  // PRINT_DBG("Hashing: %s\n", req->key().c_str());
  furc_hash(req->key().c_str(), req->key().size(), 16);

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

  // PRINT_DBG("Hashing: %s %ld\n", req->key().c_str(), size);
  furc_hash(key, size, 16);
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
