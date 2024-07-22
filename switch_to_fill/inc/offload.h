#ifndef OFFLOAD_H
#define OFFLOAD_H

#include <stdint.h>
#include "emul_ax.h"
#include "router_requests.h"

int submit_offload(ax_comp *comp, char *dst_payload);
void allocate_crs(int total_requests, ax_comp **p_comps);

#endif