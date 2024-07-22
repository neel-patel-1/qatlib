#ifndef ROUTER_REQUESTS_H
#define ROUTER_REQUESTS_H
#include "router.pb.h"
#include "emul_ax.h"
#include "ch3_hash.h"
#include "dsa_alloc.h"
#include "context_management.h"
#include "request_executors.h"
#include <string>
#include "router_request_args.h"

extern "C" {
  #include "fcontext.h"
}

#include <string>

extern int requests_completed;



void allocate_offload_requests(int total_requests, offload_request_args ***p_off_args, ax_comp *comps, char **dst_bufs);

void yielding_router_request(fcontext_transfer_t arg);
void yielding_router_request_stamp(fcontext_transfer_t arg);
void yielding_ax_router_request_breakdown_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *off_times, uint64_t *yield_to_resume_times, uint64_t *hash_times, int idx);
void yielding_ax_router_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *exetime, int start_idx);

void blocking_router_request(fcontext_transfer_t arg);
void blocking_router_request_stamp(fcontext_transfer_t arg);
void blocking_ax_router_request_breakdown_test(
  int requests_sampling_interval, int total_requests,
  uint64_t *off_times, uint64_t *wait_times, uint64_t *hash_times, int idx);
void blocking_ax_router_closed_loop_test(int requests_sampling_interval, int total_requests,
  uint64_t *exetime, int idx);

void cpu_router_request(fcontext_transfer_t arg);
void cpu_router_request_stamp(fcontext_transfer_t arg);
void cpu_router_request_breakdown(int requests_sampling_interval,
  int total_requests, uint64_t *deser_times, uint64_t *hash_times, int idx);
void cpu_router_closed_loop_test(int requests_sampling_interval,
  int total_requests, uint64_t *exetimes, int idx);


void serialize_request(router::RouterRequest *req, std::string *serialized);

void allocate_pre_deserialized_payloads(int total_requests, char ***p_dst_bufs, std::string query);
void allocate_pre_deserialized_dsa_payloads(int total_requests, char ***p_dst_bufs, std::string query);


#endif