#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <pthread.h>

#include "config.h"
#include "statefile.h"
#include "render.h"
#include "mutex_helpers.h"

void
usage(void) {
	fputs("nebula2 needs the name of a config file as 1st argument.\n", stderr);
}

/* Data that is shared between all processes. */
typedef struct {
	uint32_t* map;
	uint32_t  jobs_todo;

	pthread_mutex_t* jobrq_get_mu;
	pthread_mutex_t* jobrq_set_mu;
	int              jobrq;
} nebula_data_t;

/* Set the jobrq variable (e.g. request a new job) */
void
jobrq_set(nebula_data_t* nd, int val) {
	pthread_mutex_lock(nd->jobrq_set_mu);
	nd->jobrq = val;
	pthread_mutex_unlock(nd->jobrq_get_mu);
}

/* Get the next jobrq value */
int
jobrq_get(nebula_data_t* nd) {
	int rv;

	pthread_mutex_lock(nd->jobrq_get_mu);
	rv = nd->jobrq;
	pthread_mutex_unlock(nd->jobrq_set_mu);
	return rv;
}

nebula_data_t*
nebula_data_create(config_t* conf) {
	size_t         mapsize;
	nebula_data_t* nd = NULL;

	if(!(nd = malloc(sizeof(nebula_data_t)))) {
		return NULL;
	}

	mapsize = conf->width * conf->height * conf->iters_n;

	nd->map          = NULL;
	nd->jobrq_set_mu = NULL;
	nd->jobrq_get_mu = NULL;

	if(!(nd->jobrq_get_mu = mutex_create())) {
		fputs("Could not init jobrq_get_mu mutex.\n", stderr);
		goto failed;
	}
	if(!(nd->jobrq_set_mu = mutex_create())) {
		fputs("Could not init jobrq_set_mu mutex.\n", stderr);
		goto failed;
	}
	/* The set mutex needs to be initially unlocked, so one worker can start requesting jobs. */
	pthread_mutex_unlock(nd->jobrq_set_mu);

	if(!(nd->map = malloc(sizeof(uint32_t) * mapsize))) {
		fputs("Could not allocate memory for map.\n", stderr);
		goto failed;
	}

	return nd;

failed:
	if(nd->jobrq_set_mu) {
		mutex_destroy(nd->jobrq_set_mu);
	}
	if(nd->jobrq_get_mu) {
		mutex_destroy(nd->jobrq_get_mu);
	}
	if(nd->map) {
		free(nd->map);
	}
	return NULL;
}

void
nebula_data_destroy(nebula_data_t* nd) {
	if(nd->map) {
		free(nd->map);
	}
	mutex_destroy(nd->jobrq_set_mu);
	mutex_destroy(nd->jobrq_get_mu);
	free(nd);
}

/* Data of a single worker */
typedef struct {
	int              id;
	pthread_mutex_t* mu;
	int              ok;

	config_t*      conf;
	nebula_data_t* nd;

	pthread_t thread;
	int       thread_started;
} worker_data_t;

/* The background worker */
void*
worker(void* _wd) {
	worker_data_t* wd = _wd;
	nebula_data_t* nd = wd->nd;

	for(;; ) {
		jobrq_set(nd, wd->id);
		pthread_mutex_lock(wd->mu);
		if(!wd->ok) {
			return NULL;
		}
		/* TODO: Do magic... */
	}
}

/* Init and run a worker */
int
worker_init(worker_data_t* wd, int id, config_t* conf, nebula_data_t* nd) {
	wd->id             = id;
	wd->ok             = 1;
	wd->conf           = conf;
	wd->nd             = nd;
	wd->thread_started = 0;

	if(!(wd->mu = mutex_create())) {
		return 0;
	}

	if(pthread_create(&(wd->thread), NULL, worker, wd) != 0) {
		mutex_destroy(wd->mu);
		return 0;
	}

	wd->thread_started = 1;
	return 1;
}

void
worker_cleanup(worker_data_t* wd) {
	if(wd->thread_started) {
		pthread_join(wd->thread, NULL);
	}
	mutex_destroy(wd->mu);
}

/* Global reference to shared data (needed by sighandler) */
nebula_data_t* global_nd;

void
sighandler(int sig) {
	switch(sig) {
	case SIGINT:
		jobrq_set(global_nd, -1);
		break;
	case SIGUSR1:
		printf("Jobs todo: %d\n", global_nd->jobs_todo);
		break;
	}
}

int
setup_sighandler(void) {
	struct sigaction action;
	action.sa_handler = sighandler;
	action.sa_flags   = SA_RESTART;
	sigemptyset(&action.sa_mask);

	if(sigaction(SIGINT, &action, NULL) != 0) {
		return 0;
	}
	if(sigaction(SIGUSR1, &action, NULL) != 0) {
		return 0;
	}
	return 1;
}

int
nebula2(config_t* conf) {
	int            rv = 1;
	nebula_data_t* nd = NULL;
	uint32_t       jobs_done;
	worker_data_t* workers;
	int            i;
	int            rq;
	int            workers_alive = 0;

	if(!(nd = nebula_data_create(conf))) {
		goto tidyup;
	}
	global_nd = nd;

	if(!state_load(conf, nd->map, &jobs_done)) {
		fprintf(stderr, "Error while loading state: %s\n", strerror(errno));
		goto tidyup;
	}
	nd->jobs_todo = conf->jobs - jobs_done;

	if(!(workers = calloc(conf->procn, sizeof(worker_data_t)))) {
		fputs("Could not allocate memory for worker data.\n", stderr);
		goto tidyup;
	}
	for(i = 0; i < conf->procn; i++) {
		if(!(worker_init(&(workers[i]), i, conf, nd))) {
			fputs("Could not init worker.\n", stderr);
			goto tidyup;
		}
		workers_alive++;
	}

	if(!setup_sighandler()) {
		fprintf(stderr, "Error while configuring signals: %s\n", strerror(errno));
		goto tidyup;
	}

	/* We need to manually unlock the set mutex once, so the first worker is allowed to set jobrq. */
	while(nd->jobs_todo > 0) {
		rq = jobrq_get(nd);
		if(rq < 0) {
			break;
		}

		workers[rq].ok = 1;
		(nd->jobs_todo)--;
		pthread_mutex_unlock(workers[rq].mu);
	}

	if(!(state_save(conf, nd->map, conf->jobs - nd->jobs_todo))) {
		fprintf(stderr, "Error while saving state: %s\n", strerror(errno));
		goto tidyup;
	}

	rv = render(conf, nd->map) ? 0 : 1;

tidyup:
	while(workers_alive > 0) {
		rq = jobrq_get(nd);
		if(rq >= 0) {
			workers[rq].ok = 0;
			pthread_mutex_unlock(workers[rq].mu);
			workers_alive--;
		}
	}

	if(nd) {
		nebula_data_destroy(nd);
	}

	if(workers) {
		for(i = 0; i < conf->procn; i++) {
			worker_cleanup(&(workers[i]));
		}
		free(workers);
	}
	return rv;
}

int
main(int argc, char** argv) {
	int       rv   = 1;
	config_t* conf = NULL;

	if(argc < 2) {
		usage();
		goto tidyup;
	}

	if(!conf_load(argv[1], &conf)) {
		goto tidyup;
	}

	rv = nebula2(conf);
tidyup:
	if(conf) {
		conf_destroy(conf);
	}

	return rv;
}
