#include "dsa_alloc.h"
#include "print_utils.h"
#include "timer_utils.h"

#include <cstdlib>
#include <cstring>



typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[64 - sizeof(int) - sizeof(struct _node *)];
} node;

bool gDebugParam = true;

node *build_llc_ll(int length){
  node *cpy_src = (node *)malloc(sizeof(node));
  node *cpy_dst = (node *)malloc(sizeof(node));
  node *tgt = (node *)malloc(sizeof(node));
  node *head = cpy_dst;

  int i;
  for(i=0; i<length-1; i++){
    cpy_src->docID = i;
    cpy_src->next = tgt;

    dsa_llc_realloc(cpy_dst, cpy_src, sizeof(node));
    cpy_dst = tgt;
    tgt = (node *)malloc(sizeof(node));
  }
  cpy_src->docID = length-1; /* left with a hanging tgt to cpy to*/
  cpy_src->next = NULL;
  dsa_llc_realloc(cpy_dst, cpy_src, sizeof(node));

  free(cpy_src);

  return head;
}

node *build_host_ll(int length){
  node *head = (node *)malloc(sizeof(node));
  node *cur = head;
  node *next = NULL;
  int i;
  for(i=0; i<length-1; i++){
    cur->docID = i;
    next = (node *)malloc(sizeof(node));
    cur->next = next;
    cur = next;
  }
  cur->docID = length-1;
  cur->next = NULL;

  return head;
}

void free_ll(node *head){
  node *cur = head;
  node *next = NULL;
  while(cur != NULL){
    next = cur->next;
    free(cur);
    cur = next;
  }
}

static inline void ll_simple(node *head){
  node *cur = head;
  while(cur != NULL){
    cur = cur->next;
  }

}

static inline int dot_product(uint8_t *a, uint8_t *b, int size){
  int i;
  int sum = 0;
  for(i=0; i<size; i++){
    sum += a[i] * b[i];
  }
  return sum;
}

static inline void ll_dynamic_rank(node *head){
  node *cur = head;
  int sums[512];
  int idx= 0;
  while(cur != NULL){
    int sum = dot_product(cur->padding,
      cur->padding + sizeof(cur->padding)/2, sizeof(cur->padding)/2);
    cur = cur->next;

    sums[idx++] = sum;
  }

}

int main(){

  node *head;
  int num_nodes = 512;
  {
    time_code_region(
      head = build_llc_ll(num_nodes),
      ll_simple(head),
      free_ll(head),
      1000
    );

  }

  {
    time_code_region(
      head = build_host_ll(num_nodes),
      ll_simple(head),
      free_ll(head),
      1000
    );
  }

  {
    time_code_region(
      head = build_llc_ll(num_nodes),
      ll_dynamic_rank(head),
      free_ll(head),
      1000
    );

  }

  {
    time_code_region(
      head = build_host_ll(num_nodes),
      ll_dynamic_rank(head),
      free_ll(head),
      1000
    );
  }

  return 0;
}