#ifndef RPS
#define RPS

#include <stdint.h>
#include <cstddef>
#include "print_utils.h"

void calculate_rps_from_samples(
  uint64_t *sampling_interval_completion_times,
  int sampling_intervals,
  int requests_sampling_interval,
  uint64_t cycles_per_sec);

#endif