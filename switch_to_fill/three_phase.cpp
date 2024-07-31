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
#include "decompress_and_hash_request.hpp"
#include "dsa_offloads.h"
#include "submit.hpp"

#include "probe_point.h"
#include "filler_antagonist.h"
#include "filler_hash.h"
#include "posting_list.h"
#include "wait.h"
#include "payload_gen.h"
#include "pointer_chase.h"

int input_size = 16384;
int host_buf_bytes_acc = input_size;
int ax_buf_bytes_acc = 0;
int stride = 0;


void alloc_offload_serialized_access_args(
  int total_requests,
  timed_offload_request_args*** p_off_args,
  ax_comp *comps,
  uint64_t *ts0,
  uint64_t *ts1,
  uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));

  int num_nodes = input_size / sizeof(node);

  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->src_payload = (char *)create_random_chain(input_size); /* host buf to chase, does not get copied */
    off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->aux_payload =
      (char *)create_random_chain_starting_at(input_size,
        (void **) (off_args[i]->dst_payload)); /* ax buf buf to chase after copy */
    off_args[i]->src_size =  host_buf_bytes_acc; /* parameter describing number of bytes to access from host buf */
    off_args[i]->dst_size = ax_buf_bytes_acc; /* parameter describing number of bytes to access from ax buf*/
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
    off_args[i]->id = i;
  }

  *p_off_args = off_args;
}

void alloc_offload_linear_access_args(
  int total_requests,
  timed_offload_request_args*** p_off_args,
  ax_comp *comps,
  uint64_t *ts0,
  uint64_t *ts1,
  uint64_t *ts2,
  uint64_t *ts3
){

  timed_offload_request_args **off_args =
    (timed_offload_request_args **)malloc(total_requests * sizeof(timed_offload_request_args *));

  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size);
    off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->src_size =  host_buf_bytes_acc; /* parameter describing number of bytes to access from host buf */
    off_args[i]->dst_size = ax_buf_bytes_acc; /* parameter describing number of bytes to access from ax buf*/
    off_args[i]->aux_size = stride; /* parameter notify the request of the stride*/
    off_args[i]->desc = (struct hw_desc *)malloc(sizeof(struct hw_desc));
    off_args[i]->comp = &comps[i];
    off_args[i]->ts0 = ts0;
    off_args[i]->ts1 = ts1;
    off_args[i]->ts2 = ts2;
    off_args[i]->ts3 = ts3;
    off_args[i]->id = i;
  }

  *p_off_args = off_args;
}

void free_offload_linear_access_args(
  int total_requests,
  timed_offload_request_args*** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]);
  }
  free(off_args);
}

void free_offload_serialized_access_args(
  int total_requests,
  timed_offload_request_args*** p_off_args
){
  timed_offload_request_args **off_args = *p_off_args;
  for(int i = 0; i < total_requests; i++){
    free(off_args[i]->src_payload);
    free(off_args[i]->dst_payload);
    free(off_args[i]);
  }
  free(off_args);
}

void blocking_offload_and_serial_access_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
 void **host_pchase_st = (void **)args->src_payload;
 void **memcpy_src = (void **)args->aux_payload;
 void **ax_pchase_st = (void **)args->dst_payload;
 int num_pre_accesses = input_size / sizeof(void *);
 int num_ax_accesses = args->dst_size / sizeof(void *);
 int num_host_accesses = args->src_size / sizeof(void *);

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;


 /*access the whole host buf to start*/
  ts0[id] = sampleCoderdtsc();
  chase_pointers(host_pchase_st, num_pre_accesses);

  ts1[id] = sampleCoderdtsc();
  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)memcpy_src, (uint64_t)ax_pchase_st,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);
  spin_on(comp);

  ts2[id] = sampleCoderdtsc();
  LOG_PRINT(LOG_DEBUG, "%d HostAccesses\n" , num_host_accesses);
  chase_pointers(host_pchase_st, num_host_accesses);
  LOG_PRINT(LOG_DEBUG, "%d AXAccesses\n" , num_ax_accesses);
  chase_pointers(ax_pchase_st, num_ax_accesses);


  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}

void yielding_offload_and_linear_access_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
 char *pre_buf = (char *)args->src_payload;
 char *ax_dst_buf = (char *)args->dst_payload;
 int pre_buf_size = input_size ;
 int ax_bytes_accessed = args->dst_size;
 int host_bytes_accessed = args->src_size;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;


 /*access the whole host buf to start*/
  ts0[id] = sampleCoderdtsc();

  for(int i=0; i < pre_buf_size; i+=64){
    pre_buf[i] = i;
  }

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)pre_buf, (uint64_t)ax_dst_buf,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);

  ts1[id] = sampleCoderdtsc();

  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();

  LOG_PRINT(LOG_DEBUG, "%d HostBytesAccesses\n" , host_bytes_accessed);
  for(int i=0; i < host_bytes_accessed; i+=stride){
    pre_buf[i] = i;
  }
  LOG_PRINT(LOG_DEBUG, "%d AXBytesAccesses\n" , ax_bytes_accessed);
  for(int i=0; i < ax_bytes_accessed; i+=stride){
    ax_dst_buf[i] = i;
  }

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}

void blocking_offload_and_linear_access_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
 char *pre_buf = (char *)args->src_payload;
 char *ax_dst_buf = (char *)args->dst_payload;
 int pre_buf_size = input_size ;
 int ax_bytes_accessed = args->dst_size;
 int host_bytes_accessed = args->src_size;
 int stride = args->aux_size;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;


 /*access the whole host buf to start*/
  ts0[id] = sampleCoderdtsc();

  for(int i=0; i < pre_buf_size; i+=64){
    pre_buf[i] = i;
  }

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)pre_buf, (uint64_t)ax_dst_buf,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);

  ts1[id] = sampleCoderdtsc();

  spin_on(comp);

  ts2[id] = sampleCoderdtsc();

  LOG_PRINT(LOG_DEBUG, "%d HostBytesAccesses\n" , host_bytes_accessed);
  for(int i=0; i < host_bytes_accessed; i+=stride){
    pre_buf[i] = i;
  }
  LOG_PRINT(LOG_DEBUG, "%d AXBytesAccesses\n" , ax_bytes_accessed);
  for(int i=0; i < ax_bytes_accessed; i+=stride){
    ax_dst_buf[i] = i;
  }

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}


void yielding_offload_and_serial_access_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;
 void **host_pchase_st = (void **)args->src_payload;
 void **memcpy_src = (void **)args->aux_payload;
 void **ax_pchase_st = (void **)args->dst_payload;
 int num_pre_accesses = input_size / sizeof(void *);
 int num_ax_accesses = args->dst_size / sizeof(void *);
 int num_host_accesses = args->src_size / sizeof(void *);

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;


 /*access the whole host buf to start*/
  ts0[id] = sampleCoderdtsc();
  chase_pointers(host_pchase_st, num_pre_accesses);

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)memcpy_src, (uint64_t)ax_pchase_st,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);

  ts1[id] = sampleCoderdtsc();

  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();

  LOG_PRINT(LOG_DEBUG, "%d HostAccesses\n" , num_host_accesses);
  chase_pointers(host_pchase_st, num_host_accesses);
  LOG_PRINT(LOG_DEBUG, "%d AXAccesses\n" , num_ax_accesses);
  chase_pointers(ax_pchase_st, num_ax_accesses);

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);

}


int gLogLevel = LOG_PERF;
bool gDebugParam = false;
int main(int argc, char **argv){


  int wq_id = 0;
  int dev_id = 0;
  int wq_type = SHARED;
  int rc;
  int itr = 100;
  int total_requests = 1000;
  int opt;
  bool no_latency = false;
  bool no_thrpt = false;

  while((opt = getopt(argc, argv, "y:s:j:t:i:r:s:q:d:hf")) != -1){
    switch(opt){
      case 't':
        total_requests = atoi(optarg);
        break;
      case 'i':
        itr = atoi(optarg);
        break;
      case 'q':
        input_size = atoi(optarg);
        break;
      case 'o':
        no_latency = true;
        break;
      case 'h':
        no_thrpt = true;
        break;
      case 'd':
        dev_id = atoi(optarg);
        break;
      case 'f':
        gLogLevel = LOG_DEBUG;
        break;
      case 'j':
        host_buf_bytes_acc = atoi(optarg);
        break;
      case 's':
        ax_buf_bytes_acc = atoi(optarg);
        break;
      case 'y':
        stride = atoi(optarg);
        break;
      default:
        break;
    }
  }

  initialize_dsa_wq(dev_id, wq_id, wq_type);

  LOG_PRINT(LOG_PERF, "Input size: %d\n", input_size);
  if(!no_latency){
    run_blocking_offload_request_brkdown(
      blocking_offload_and_serial_access_stamped,
      alloc_offload_serialized_access_args,
      free_offload_serialized_access_args,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_serial_access_stamped,
      hash_interleaved, /* upper bound on yield perf */
      alloc_offload_serialized_access_args,
      free_offload_serialized_access_args,
      itr,
      total_requests
    );
    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_serial_access_stamped,
      antagonist_interleaved,
      alloc_offload_serialized_access_args,
      free_offload_serialized_access_args,
      itr,
      total_requests
    );

    stride = 64;

    run_blocking_offload_request_brkdown(
      blocking_offload_and_linear_access_stamped,
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_linear_access_stamped,
      hash_interleaved, /* upper bound on yield perf */
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_linear_access_stamped,
      antagonist_interleaved, /* upper bound on yield perf */
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );

    stride = 1;

    run_blocking_offload_request_brkdown(
      blocking_offload_and_linear_access_stamped,
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_linear_access_stamped,
      hash_interleaved, /* upper bound on yield perf */
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_offload_and_linear_access_stamped,
      antagonist_interleaved, /* upper bound on yield perf */
      alloc_offload_linear_access_args,
      free_offload_linear_access_args,
      itr,
      total_requests
    );
  }

  free_dsa_wq();
  return 0;

}