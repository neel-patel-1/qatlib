#ifndef WALKER_H
#define WALKER_H
extern "C" {
  #include "fcontext.h"
}
#include "router_request_args.h"
#include "emul_ax.h"
#include "print_utils.h"
#include "idxd.h"
#include <stdint.h>

extern std::atomic<uint64_t> total_offloads; /* Exported total_offloads used for monitoring at request side*/

void cpu_simple_ranker_request_stamped(fcontext_transfer_t arg);
void cpu_simple_ranker_request(fcontext_transfer_t arg);

void blocking_simple_ranker_request_stamped(fcontext_transfer_t arg);
void blocking_simple_ranker_request(fcontext_transfer_t arg);

void yielding_simple_ranker_request(fcontext_transfer_t arg);
void yielding_simple_ranker_request_stamped(fcontext_transfer_t arg);

#endif