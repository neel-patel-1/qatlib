#include "posting_list.h"

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

void allocate_pre_intersected_posting_lists_llc(
  int total_requests,
  char ***p_posting_list_heads_arr
  )
{
  *p_posting_list_heads_arr = (char **)malloc(sizeof(char *) * total_requests);
  for(int i=0; i<total_requests; i++){
    (*p_posting_list_heads_arr)[i] = (char *)build_llc_ll(10);
  }

}

void free_pre_intersected_posting_lists_llc(
  int total_requests,
  char ***p_posting_list_heads_arr
  )
{
  for(int i=0; i<total_requests; i++){
    free_ll((node *)((*p_posting_list_heads_arr)[i]));
  }
  free(*p_posting_list_heads_arr);
}