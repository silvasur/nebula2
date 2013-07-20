#include <stdlib.h>
#include <pthread.h>

/* Creating a locked mutex */
pthread_mutex_t*
mutex_create() {
	pthread_mutex_t* mu;

	if(!(mu = malloc(sizeof(pthread_mutex_t)))) {
		return NULL;
	}

	if(pthread_mutex_init(mu, NULL) != 0) {
		free(mu);
		return NULL;
	}

	pthread_mutex_trylock(mu);

	return mu;
}

/* Destroy a mutex created by mutex_create */
void
mutex_destroy(pthread_mutex_t* mu) {
	if(mu) {
		pthread_mutex_unlock(mu);
		pthread_mutex_destroy(mu);
	}
}
