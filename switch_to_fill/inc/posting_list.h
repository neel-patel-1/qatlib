#ifndef POSTING_LIST_H
#define POSTING_LIST_H
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "dsa_alloc.h"
#include "print_utils.h"

extern int pl_len;

typedef struct _node{
  int docID;
  struct _node *next;
  uint8_t padding[40];
} node;

node *build_llc_ll(int length);
node *build_host_ll(int length);
void free_ll(node *head);

void allocate_pre_intersected_posting_lists_llc(int total_requests,
  char ***p_posting_list_heads_arr);
void free_pre_intersected_posting_lists_llc(
  int total_requests,
  char ***p_posting_list_heads_arr
  );

void allocate_posting_lists(int total_requests,
  char ****p_arr_posting_list_heads_arrs);
void free_posting_lists(int total_requests,
  char ****p_arr_posting_list_heads_arrs);

static void ll_simple(node *head);
static inline void intersect_posting_lists(node *intersected, node *head1, node *head2);
static int dot_product(uint8_t *a, uint8_t *b, int size);
static void ll_dynamic_rank(node *head);

#include "inline/posting_list.ipp"
#endif