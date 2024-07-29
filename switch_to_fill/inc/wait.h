#ifndef WAIT_H
#define WAIT_H
#include "emul_ax.h"
#include "x86intrin.h"
static inline void spin_on(ax_comp *comp){
  while(comp->status == IAX_COMP_NONE){  }
}

static inline void pause_on(ax_comp *comp){
while(comp->status == IAX_COMP_NONE){
    _mm_pause();
  }
}

// static inline void mwait_on(ax_comp *comp){
//   while(comp->status == IAX_COMP_NONE){
//     _mm_monitor(&comp->status, 0, 0);
//     _mm_mwait(0, 0);
//   }
// }

#endif // WAIT_H