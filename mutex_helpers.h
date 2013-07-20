#ifndef _nebula2_mutex_helpers_h_
#define _nebula2_mutex_helpers_h_

#include <pthread.h>

/* Creating a locked mutex */
extern pthread_mutex_t* mutex_create();

/* Destroy a mutex created by mutex_create */
extern void mutex_destroy(pthread_mutex_t* mu);

#endif
