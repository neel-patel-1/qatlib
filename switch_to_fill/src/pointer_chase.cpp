#include "pointer_chase.h"
#include "print_utils.h"
volatile void *chase_pointers_global;
void chase_pointers(void **memory, int count){
  void ** p = (void **)memory;
  while (count -- > 0) {
    p = (void **) *p;
  }
  chase_pointers_global = *p;
  if(count > 0){
    LOG_PRINT( LOG_DEBUG, "Remaining count: %d\n", count);
  }
}

void debug_chain(void **memory){
  void ** p = memory;
  size_t count = 0;
  LOG_PRINT( LOG_DEBUG, "chain at %p:\n", memory);
  do {
    LOG_PRINT( LOG_DEBUG, "  %p -> %p\n", p, *p);
    p = (void **) *p;
    count++;
  } while (p != memory);
  LOG_PRINT( LOG_DEBUG, "# of pointers in chain: %lu\n", count);


}