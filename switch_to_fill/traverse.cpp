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

node *glb_ll_head = NULL; /* in case filler needs to restart */
node *glb_node_ptr = NULL;

void lltraverse_interleaved(fcontext_transfer_t arg){
  node *cur = glb_node_ptr;
  bool traversal_in_progress = true;

  init_probe(arg); // init sched ctx and sig

  while (1) {
    if(is_signal_set(p_sig)){ /*check signal */
      cur = NULL;
      traversal_in_progress = false; /* stop the traversal */
      fcontext_transfer_t parent_resume =
        fcontext_swap( sched_ctx.prev_context, NULL); /* switch to sched */
      p_sig = (preempt_signal *)parent_resume.data; /*set the new signal returned from the scheduler */
    }

    if(cur == NULL){ /* the cur can be null*/
      if(traversal_in_progress){ // we're either back from a yield or not
        cur = glb_ll_head; // restart the traversal if we're not
      } else {
        cur = glb_node_ptr; // set to node set by main if we are
      }
    } else {
      LOG_PRINT(LOG_DEBUG, "Filler@Node: %d\n", cur->docID);
      cur = cur->next;
    }
  }

}

void alloc_offload_traverse_and_offload_args(
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

  /* allocate a global shared linked list once */
  if(glb_ll_head == NULL){
    glb_ll_head = build_host_ll(num_nodes);
  }

  for(int i = 0; i < total_requests; i++){
    off_args[i] = (timed_offload_request_args *)malloc(sizeof(timed_offload_request_args));
    off_args[i]->src_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->dst_payload = (char *)malloc(input_size * sizeof(char));
    off_args[i]->aux_payload = (char *)glb_ll_head;
    off_args[i]->src_size =  input_size;
    off_args[i]->dst_size = num_nodes;
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

void free_offload_traverse_and_offload_args(
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
  free_ll(glb_ll_head);
  glb_ll_head = NULL;
}

void yielding_traverse_and_offload_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;

  node *ll_head = (node *)args->aux_payload;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  ts0[id] = sampleCoderdtsc();// !
  node *cur = ll_head; /* traverse first half */
  while(cur != NULL ){
    LOG_PRINT(LOG_DEBUG, "Main@Node: %d\n", cur->docID);
    cur = cur->next;
  }

  glb_node_ptr = ll_head; /* let same filler know where to resume */

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);

  ts1[id] = sampleCoderdtsc();
  fcontext_swap(arg.prev_context, NULL);

  ts2[id] = sampleCoderdtsc();
  cur = ll_head; /* traverse first half */
  while(cur != NULL ){
    LOG_PRINT(LOG_DEBUG, "Main@Node: %d\n", cur->docID);
    cur = cur->next;
  }

  ts3[id] = sampleCoderdtsc();

  requests_completed ++;
  fcontext_swap(arg.prev_context, NULL);
}

void blocking_offload_and_access_stamped(fcontext_transfer_t arg){
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

void blocking_traverse_and_offload_stamped(fcontext_transfer_t arg){
  timed_offload_request_args *args = (timed_offload_request_args *)arg.data;

  char *src = args->src_payload;
  char *dst = args->dst_payload;

  node *ll_head = (node *)args->aux_payload;

  int id = args->id;
  uint64_t *ts0 = args->ts0;
  uint64_t *ts1 = args->ts1;
  uint64_t *ts2 = args->ts2;
  uint64_t *ts3 = args->ts3;
  ax_comp *comp = args->comp;
  struct hw_desc *desc = args->desc;

  ts0[id] = sampleCoderdtsc();// !
  node *cur = ll_head; /* traverse first half */
  while(cur != NULL ){
    LOG_PRINT(LOG_DEBUG, "Main@Node: %d\n", cur->docID);
    cur = cur->next;
  }

  glb_node_ptr = ll_head; /* let same filler know where to resume */

  prepare_dsa_memcpy_desc_with_preallocated_comp(
    desc, (uint64_t)src, (uint64_t)dst,
    (uint64_t)args->comp, (uint64_t)input_size
  );
  blocking_dsa_submit(dsa, desc);

  ts1[id] = sampleCoderdtsc();
  spin_on(comp);


  ts2[id] = sampleCoderdtsc();
  cur = ll_head; /* traverse first half */
  while(cur != NULL ){
    LOG_PRINT(LOG_DEBUG, "Main@Node: %d\n", cur->docID);
    cur = cur->next;
  }

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
      blocking_traverse_and_offload_stamped,
      alloc_offload_traverse_and_offload_args,
      free_offload_traverse_and_offload_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_traverse_and_offload_stamped,
      lltraverse_interleaved,
      alloc_offload_traverse_and_offload_args,
      free_offload_traverse_and_offload_args,
      itr,
      total_requests
    );

    run_yielding_interleaved_request_brkdown(
      yielding_traverse_and_offload_stamped,
      antagonist_interleaved,
      alloc_offload_traverse_and_offload_args,
      free_offload_traverse_and_offload_args,
      itr,
      total_requests
    );

  }

  free_dsa_wq();
  return 0;

}