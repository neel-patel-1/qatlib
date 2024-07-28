#ifndef GATHER_SCATTER_H
#define GATHER_SCATTER_H
#include <stdlib.h>
#include "print_utils.h"
#include <algorithm>

extern int input_size;
extern int num_accesses;

void indirect_array_gen(int **p_indirect_array);
void print_sorted_array(int *sorted_idxs, int num_accesses);
void validate_scatter_array(float *scatter_array, int *indirect_array);

#endif