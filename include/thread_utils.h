#ifndef _THREAD_UTILS
#define _THREAD_UTILS

#include <pthread.h>

void create_thread_pinned(pthread_t *thread, void *func, void *arg, int core_id);

#endif