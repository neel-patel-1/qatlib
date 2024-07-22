#ifndef ROUTER_REQUESTS_H
#define ROUTER_REQUESTS_H
#include "router.pb.h"
#include "emul_ax.h"
#include "ch3_hash.h"
#include <string>

extern "C" {
  #include "fcontext.h"
}

#include <string>

extern int requests_completed;

typedef struct _offload_request_args{
  ax_comp *comp;
  char *dst_payload;
  int id;
} offload_request_args;

typedef struct _timed_offload_request_args{
  ax_comp *comp;
  char *dst_payload;
  int id;
  uint64_t *ts0;
  uint64_t *ts1;
  uint64_t *ts2;
  uint64_t *ts3;
} timed_offload_request_args;

typedef struct _cpu_request_args{
  router::RouterRequest *request;
  std::string *serialized;
} cpu_request_args;
typedef struct _timed_cpu_request_args{
  router::RouterRequest *request;
  std::string *serialized;
  uint64_t *ts0;
  uint64_t *ts1;
  uint64_t *ts2;
  int id;
} timed_cpu_request_args;

void allocate_offload_requests(int total_requests, offload_request_args ***p_off_args, ax_comp *comps, char **dst_bufs);

void yielding_router_request(fcontext_transfer_t arg);
void yielding_router_request_stamp(fcontext_transfer_t arg);

void blocking_router_request(fcontext_transfer_t arg);
void blocking_router_request_stamp(fcontext_transfer_t arg);

void cpu_router_request(fcontext_transfer_t arg);
void cpu_router_request_stamp(fcontext_transfer_t arg);

void serialize_request(router::RouterRequest *req, std::string *serialized);

void allocate_pre_deserialized_payloads(int total_requests, char ***p_dst_bufs, std::string query);

#endif