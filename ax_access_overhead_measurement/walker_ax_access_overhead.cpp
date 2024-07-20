#include "dsa_alloc.h"
#include "print_utils.h"
#include "timer_utils.h"

#include <cstdlib>
#include <cstring>



typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[512 - sizeof(int) - sizeof(struct _node *)];
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

void free_llc_ll(node *head){
  node *cur = head;
  node *next = NULL;
  while(cur != NULL){
    next = cur->next;
    free(cur);
    cur = next;
  }
}

static inline void ll_kernel(node *head){
  node *cur = head;
  while(cur != NULL){
    cur = cur->next;
  }

}

int main(){

  node *head;
  {
    time_code_region(
      head = build_llc_ll(512),
      ll_kernel(head),
      free_llc_ll(head),
      1000
    );

  }
  // head = build_llc_ll(512);
  // ll_kernel(head);
  // free_llc_ll(head);
  return 0;
}