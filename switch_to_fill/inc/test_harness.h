#ifndef TEST_HARNESS
#define TEST_HARNESS

extern "C" {
  #include "fcontext.h"
}

#include "emul_ax.h"
#include "offload.h"
#include "context_management.h"

extern int requests_completed;

void blocking_ax_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char***),
  void (* payload_free)(int, char***),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
  );

void gpcore_closed_loop_test(
  fcontext_fn_t request_fn,
  void (* payload_allocator)(int, char****),
  void (* payload_free)(int, char****),
  int requests_sampling_interval,
  int total_requests,
  uint64_t *exetime, int idx
);

#endif