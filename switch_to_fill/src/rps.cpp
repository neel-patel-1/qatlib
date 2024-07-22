#include "rps.h"

void calculate_rps_from_samples(
  uint64_t *sampling_interval_completion_times,
  int sampling_intervals,
  int requests_sampling_interval,
  uint64_t cycles_per_sec)
{
  uint64_t avg, sum =0 ;

  for(int i=0; i<sampling_intervals; i++){
    PRINT_DBG("Sampling Interval %d: %ld\n", i,
      sampling_interval_completion_times[i+1] - sampling_interval_completion_times[i]);
    sum += sampling_interval_completion_times[i+1] - sampling_interval_completion_times[i];
  }
  avg = sum / (sampling_intervals);
  // PRINT_DBG("AverageExeTimeFor%dRequests: %ld\n", requests_sampling_interval, avg);
  PRINT_DBG("AveRPS: %f\n", (double)(requests_sampling_interval * cycles_per_sec )/ avg);
}