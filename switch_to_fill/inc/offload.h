#ifndef OFFLOAD_H
#define OFFLOAD_H

#include <stdint.h>
#include "emul_ax.h"
#include "router_requests.h"
extern "C"{
  #include "idxd.h"
}

int submit_offload(ax_comp *comp, char *dst_payload);
void allocate_crs(int total_requests, ax_comp **p_comps);

void allocate_offload_requests(int total_requests, offload_request_args ***p_off_args, ax_comp *comps, char **dst_bufs);
void allocate_timed_offload_requests(int total_requests,
  timed_offload_request_args ***p_off_args,
  ax_comp *comps, char **dst_bufs,
  uint64_t *ts0, uint64_t *ts1, uint64_t *ts2, uint64_t *ts3
  );

#endif