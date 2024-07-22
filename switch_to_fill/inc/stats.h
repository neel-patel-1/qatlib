#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

uint64_t avg_from_array(uint64_t *arr, int size);
uint64_t stddev_from_array(uint64_t *arr, int size);
uint64_t median_from_array(uint64_t *arr, int size);

#endif