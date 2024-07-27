#include "test_harness.h"
#include "print_utils.h"
#include "router_request_args.h"
#include "iaa_offloads.h"
#include "ch3_hash.h"
#include "runners.h"
#include "gpcore_compress.h"

extern "C" {
  #include "fcontext.h"
  #include "iaa.h"
  #include "accel_test.h"
  #include "iaa_compress.h"
  #include <zlib.h>
}
#include "dsa_offloads.h"
#include "submit.hpp"
#include <algorithm>

int input_size = 16384;
int num_accesses = 10;
float *feature_buf;
int *indirect_array;

void (*compute_on_input)(void *, int);

static inline void indirect_array_gen(){
  int num_feature_ent = input_size / sizeof(float);
  int max_val = num_feature_ent - 1;
  int min_val = 0;

  indirect_array = (int *)malloc(num_accesses * sizeof(int));

  for(int i=0; i<num_accesses; i++){
    int idx = (rand() % (max_val - min_val + 1)) + min_val;
    indirect_array[i] = (float)idx;
  }
}

int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){

  int sorted_idxs[num_accesses];
  int num_ents;

  input_size = 1024;
  num_ents = input_size / sizeof(float);

  indirect_array_gen();
  feature_buf
    = (float *)malloc(input_size * sizeof(float));

  memset(feature_buf, 0, input_size * sizeof(float));

  /* populate feature buf using indrecet array*/
  for(int i = 0; i < num_accesses; i++){
    feature_buf[indirect_array[i]] = 1.0;
    sorted_idxs[i] = indirect_array[i];
  }
  std::sort(sorted_idxs, sorted_idxs + num_accesses);


  /* validate feature buf entries */
  int sorted_idx = 0;
  for(int i=0; i<num_ents; i++){
    if (i == sorted_idxs[sorted_idx]){
      if (feature_buf[i] != 1.0){
        LOG_PRINT(LOG_ERR, "Error in feature buf\n");
        return -1;
      }
      sorted_idx++;
    } else {
      if (feature_buf[i] != 0.0){
        LOG_PRINT(LOG_ERR, "Error in feature buf\n");
        return -1;
      }
    }
  }

}