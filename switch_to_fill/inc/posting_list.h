#ifndef POSTING_LIST_H
#define POSTING_LIST_H
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "dsa_alloc.h"
#include "print_utils.h"

typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[64 - sizeof(int) - sizeof(struct _node *)];
} node;

node *build_llc_ll(int length);
node *build_host_ll(int length);
void free_ll(node *head);

void allocate_pre_intersected_posting_lists_llc(int total_requests, node **p_head_ptrs, int length);
static void ll_simple(node *head);

#include "inline/posting_list.ipp"
#endif