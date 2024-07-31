#ifndef ROUTER_REQUEST_ARGS_H
#define ROUTER_REQUEST_ARGS_H
#include "router.pb.h"
#include <string>
#include "emul_ax.h"

typedef struct _offload_request_args{
  ax_comp *comp;
  struct hw_desc *desc;
  char *src_payload;
  uint64_t src_size;
  uint64_t dst_size;
  int aux_size; /*users three_phase.cpp */
  char *dst_payload;
  char *aux_payload; /* users - decomp_and_scatter, memfill_and_gather */
  int id;
} offload_request_args;

typedef struct _timed_offload_request_args{
  ax_comp *comp;
  struct hw_desc *desc;
  char *src_payload;
  uint64_t src_size;
  uint64_t dst_size;
  char *dst_payload;
  char *aux_payload; /* users - decomp_and_scatter, memfill_and_gather */
  int aux_size; /*users three_phase.cpp */
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

typedef struct _gpcore_request_args{
  char **inputs;
  int id;
} gpcore_request_args;
typedef struct _timed_gpcore_request_args{
  char **inputs;
  int id;
  uint64_t *ts0;
  uint64_t *ts1;
  uint64_t *ts2;
} timed_gpcore_request_args;

#endif