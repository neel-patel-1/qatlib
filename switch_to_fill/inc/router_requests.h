#ifndef ROUTER_REQUESTS_H
#define ROUTER_REQUESTS_H
#include "router.pb.h"
#include "emul_ax.h"
#include "ch3_hash.h"

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

typedef struct _cpu_request_args{
  router::RouterRequest *request;
  std::string *serialized;
} cpu_request_args;

void allocate_offload_requests(int total_requests, offload_request_args ***p_off_args, ax_comp *comps, char **dst_bufs);

void yielding_router_request(fcontext_transfer_t arg);

void blocking_router_request(fcontext_transfer_t arg);

void cpu_router_request(fcontext_transfer_t arg);

#endif