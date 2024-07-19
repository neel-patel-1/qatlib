#ifndef LINKED_LIST
#define LINKED_LIST

#include "print_utils.h"

typedef struct _node{
  void *data;
  struct _node *next;
} node;

typedef struct _linked_list{
  node *head;
  node *tail;
  int size;
} linked_list;

linked_list *ll_init(){
  linked_list *ll = (linked_list *)malloc(sizeof(linked_list));
  ll->head = NULL;
  ll->tail = NULL;
  ll->size = 0;
  return ll;
}

void ll_insert(linked_list *ll, void *data){
  node *new_node = (node *)malloc(sizeof(node));
  new_node->data = data;
  new_node->next = NULL;
  if(ll->head == NULL){
    ll->head = new_node;
    ll->tail = new_node;
  }else{
    ll->tail->next = new_node;
    ll->tail = new_node;
  }
  ll->size++;
}

void ll_remove(linked_list *ll, void *data){
  node *cur_node = ll->head;
  node *prev_node = NULL;
  while(cur_node != NULL){
    if(cur_node->data == data){
      if(prev_node == NULL){
        ll->head = cur_node->next;
      }else{
        prev_node->next = cur_node->next;
      }
      free(cur_node);
      ll->size--;
      return;
    }
    prev_node = cur_node;
    cur_node = cur_node->next;
  }
}

void ll_free(linked_list *ll){
  node *cur_node = ll->head;
  node *next_node = NULL;
  while(cur_node != NULL){
    next_node = cur_node->next;
    free(cur_node);
    cur_node = next_node;
  }
  free(ll);
}

void ll_print(linked_list *ll){
  node *cur_node = ll->head;
  while(cur_node != NULL){
    if(cur_node->data == NULL){
      PRINT_DBG("NULL\n");
      cur_node = cur_node->next;
      continue;
    }
    PRINT_DBG("%d\n", *(int *)(cur_node->data));
    cur_node = cur_node->next;
  }
}

void populate_linked_list_ascending_values(linked_list *ll, int size){
  for(int i=0; i<size; i++){
    void *data = (void *)malloc(sizeof(int));
    *(int *)data = i;
    ll_insert(ll, data);
  }
}

#endif