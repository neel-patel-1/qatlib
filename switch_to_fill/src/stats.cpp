#include "stats.h"
#include "print_utils.h"

uint64_t avg_from_array(uint64_t *arr, int size){
  uint64_t sum = 0;
  for(int i=0; i<size; i++)
    sum += arr[i];
  return sum / size;

}

uint64_t stddev_from_array(uint64_t *arr, int size){
  uint64_t avg = avg_from_array(arr, size);
  uint64_t sum = 0;
  for(int i=0; i<size; i++)
    sum += (arr[i] - avg) * (arr[i] - avg);
  return sum / size;
}

uint64_t median_from_array(uint64_t *arr, int size){
  uint64_t *sorted = (uint64_t *)malloc(sizeof(uint64_t) * size);
  memcpy(sorted, arr, sizeof(uint64_t) * size);
  std::sort(sorted, sorted + size);
  uint64_t median = sorted[size / 2];
  free(sorted);
  return median;
}

void print_mean_median_stdev(uint64_t *arr, int size, const char *name){
  uint64_t mean = avg_from_array(arr, size);
  uint64_t median = median_from_array(arr, size);
  uint64_t stdev = stddev_from_array(arr, size);
  PRINT( "%s Mean: %lu Median: %lu Stddev: %lu\n", name, mean, median, stdev);
}