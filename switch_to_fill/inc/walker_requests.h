#ifndef WALKER_H
#define WALKER_H
extern "C" {
  #include "fcontext.h"
}
#include "router_request_args.h"

void cpu_simple_ranker_request_stamped(fcontext_transfer_t arg);
void cpu_simple_ranker_request(fcontext_transfer_t arg);

void blocking_simple_ranker_request_stamped(fcontext_transfer_t arg);
void blocking_simple_ranker_request(fcontext_transfer_t arg);

void yielding_simple_ranker_request(fcontext_transfer_t arg);
#endif