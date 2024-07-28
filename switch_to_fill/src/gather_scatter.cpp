#include "gather_scatter.h"

void indirect_array_gen(int **p_indirect_array){
  int num_feature_ent = input_size / sizeof(float);
  int max_val = num_feature_ent - 1;
  int min_val = 0;

  int *indirect_array = (int *)malloc(num_accesses * sizeof(int));

  for(int i=0; i<num_accesses; i++){
    int idx = (rand() % (max_val - min_val + 1)) + min_val;
    indirect_array[i] = (float)idx;
  }
  *p_indirect_array = (int *)indirect_array;
}

void print_sorted_array(int *sorted_idxs, int num_accesses){
  for(int i = 0; i < num_accesses; i++){
    LOG_PRINT(LOG_DEBUG, "sorted_idxs[%d] = %d\n", i, sorted_idxs[i]);
  }
}

void validate_scatter_array(float *scatter_array, int *indirect_array){
  int num_ents;
  num_ents = input_size / sizeof(float);
  int sorted_idxs[num_accesses];
  std::sort(indirect_array, indirect_array + num_accesses);
  int sorted_idx = 0;
  for(int i=0; i<num_ents; i++){
    if (i == indirect_array[sorted_idx]){
      if (scatter_array[i] != 1.0){
        print_sorted_array(indirect_array, num_accesses);
        LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
          sorted_idx, i, scatter_array[i], indirect_array[sorted_idx]);
        return;
      }
      while (indirect_array[sorted_idx] == i){
        sorted_idx++;
      }
    } else if (scatter_array[i] != 0.0){
        print_sorted_array(indirect_array, num_accesses);
        LOG_PRINT(LOG_ERR, "Error in feature buf sidx: %d idx: %d ent: %f next_expected_one_hot %d\n",
          sorted_idx, i, scatter_array[i], indirect_array[sorted_idx]);
        return;

    }
  }
}