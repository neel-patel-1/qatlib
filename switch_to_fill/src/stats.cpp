#include "stats.h"

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
