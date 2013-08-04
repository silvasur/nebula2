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

#include "SFMT/SFMT.h"

/* Bailout is currently hard coded... perhaps we should move this to the config file? */
#define BAILOUT 8

inline static long
fast_floor(double input) {
	return (long) input - (input > 0 ? 0 : 1);
}

inline static void
calc_mandelbrot(double cx, double cy, double* zx, double* zy) {
	double ty;
	ty  = (*zy) * (*zy) - (*zx) * (*zx) + cy;
	*zx = 2.0 * (*zy) * (*zx) + cx;
	*zy = ty;
}

void
precalc_nebula_params(config_t* conf, double* conv, double* mult_x, double* mult_y, int* hw, int* hh) {
	*conv = ((conf->width < conf->height) ? conf->width : conf->height) / 4.0;

	*mult_x = (double) (0.8 * conf->width  / *conv) / (UINT32_MAX / 2.0);
	*mult_y = (double) (0.8 * conf->height / *conv) / (UINT32_MAX / 2.0);

	*hw = conf->width / 2;
	*hh = conf->height / 2;
}

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

typedef struct {
	int x, y;
} pos_t;

/* Data of a single worker */
typedef struct {
	int              id;
	pthread_mutex_t* mu;
	int              ok;

	pos_t* pointlist;

	sfmt_t* sfmt_state;

	config_t*      conf;
	nebula_data_t* nd;

	pthread_t thread;
	int       thread_started;
} worker_data_t;

inline static pos_t
calc_pos(double zx, double zy, size_t width, size_t height, double conv, int hw, int hh) {
	pos_t rv;
	rv.x = fast_floor(zx * conv) + hw;
	rv.y = fast_floor(zy * conv) + hh;

	if((rv.x < 0) || (rv.y < 0) || (rv.x >= width) || (rv.y >= height)) {
		rv.x = -1;
	}

	return rv;
}

/* The background worker */
void*
worker(void* _wd) {
	/* Precalculated data (scaling factors etc.) */
	double conv, mult_x, mult_y;
	int    hw, hh;

	/* Mandelbrot point vars */
	uint64_t xy;
	double   cx, cy, zx, zy;

	/* Misc... */
	int    todo;
	int    iter, maxiter, mii;
	size_t off, mapsize;
	pos_t  pos;

	/* Aliases */
	worker_data_t* wd         = _wd;
	nebula_data_t* nd         = wd->nd;
	config_t*      conf       = wd->conf;
	sfmt_t*        sfmt_state = wd->sfmt_state;
	pos_t*         pointlist;
	uint32_t*      map    = nd->map;
	size_t         width  = conf->width;
	size_t         height = conf->height;

	precalc_nebula_params(conf, &conv, &mult_x, &mult_y, &hw, &hh);
	mapsize = width * height;
	maxiter = conf->iters[conf->iters_n - 1];

	for(;; ) {
		jobrq_set(nd, wd->id);
		pthread_mutex_lock(wd->mu);
		if(!wd->ok) {
			return NULL;
		}

		for(todo = conf->jobsize; todo--; ) {
			xy = sfmt_genrand_uint64(sfmt_state);
			cx = ((int32_t) (xy >> 32)) * mult_x;
			cy = ((int32_t) (xy & UINT32_MAX)) * mult_y;
			zx = zy = .0;

			pointlist = wd->pointlist;
			for(iter = 0; iter < maxiter; iter++) {
				calc_mandelbrot(cx, cy, &zx, &zy);
				*(pointlist++) = calc_pos(zx, zy, width, height, conv, hw, hh);
				if((zx * zx) + (zy * zy) > BAILOUT) {
					for(mii = 0; iter > conf->iters[mii]; mii++) {}
					off = mii * mapsize;
					do {
						/*
						 * To be 100% accurate, we would need to synchronize the access to the map here.
						 * We ignore this, since collision should be seldom.
						 */
						pos = *(--pointlist);
						if(pos.x < 0) {
							continue;
						}
						map[off + width * pos.y + pos.x]++;
					} while(iter-- > 0);
					break;
				}
			}
		}
	}
}

sfmt_t*
init_sfmt(void) {
	sfmt_t*  sfmt_state = NULL;
	uint32_t seed;
	FILE*    fh = NULL;

	if(!(sfmt_state = malloc(sizeof(sfmt_t)))) {
		goto failed;
	}

	if(!(fh = fopen("/dev/urandom", "rb"))) {
		goto failed;
	}

	if(fread(&seed, sizeof(uint32_t), 1, fh) != 1) {
		goto failed;
	}

	fclose(fh);

	sfmt_init_gen_rand(sfmt_state, seed);

	return sfmt_state;
failed:
	if(sfmt_state) {
		free(sfmt_state);
	}
	if(fh) {
		fclose(fh);
	}
	return NULL;
}

/* Init and run a worker */
int
worker_init(worker_data_t* wd, int id, config_t* conf, nebula_data_t* nd) {
	wd->id             = id;
	wd->ok             = 1;
	wd->conf           = conf;
	wd->nd             = nd;
	wd->thread_started = 0;

	wd->mu         = NULL;
	wd->pointlist  = NULL;
	wd->sfmt_state = NULL;

	if(!(wd->sfmt_state = init_sfmt())) {
		goto failed;
	}

	if(!(wd->mu = mutex_create())) {
		goto failed;
	}

	if(!(wd->pointlist = malloc(sizeof(pos_t) * conf->iters[conf->iters_n - 1]))) {
		goto failed;
	}

	if(pthread_create(&(wd->thread), NULL, worker, wd) != 0) {
		goto failed;
	}

	wd->thread_started = 1;
	return 1;

failed:
	if(wd->mu) {
		mutex_destroy(wd->mu);
	}
	if(wd->pointlist) {
		free(wd->pointlist);
	}
	if(wd->sfmt_state) {
		free(wd->sfmt_state);
	}
	return 0;
}

void
worker_cleanup(worker_data_t* wd) {
	if(wd->thread_started) {
		pthread_join(wd->thread, NULL);
	}
	if(wd->pointlist) {
		free(wd->pointlist);
	}
	if(wd->sfmt_state) {
		free(wd->sfmt_state);
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

void
stop_workers(nebula_data_t* nd, worker_data_t* workers, int* workers_alive) {
	int rq;

	while(*workers_alive > 0) {
		rq = jobrq_get(nd);
		if(rq >= 0) {
			workers[rq].ok = 0;
			pthread_mutex_unlock(workers[rq].mu);
			(*workers_alive)--;
		}
	}
}

int
nebula2(config_t* conf) {
	int            rv = 1;
	nebula_data_t* nd = NULL;
	uint32_t       jobs_done;
	worker_data_t* workers = NULL;
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

	if(!(workers = calloc(conf->threads, sizeof(worker_data_t)))) {
		fputs("Could not allocate memory for worker data.\n", stderr);
		goto tidyup;
	}
	for(i = 0; i < conf->threads; i++) {
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

	while(nd->jobs_todo > 0) {
		rq = jobrq_get(nd);
		if(rq < 0) {
			break;
		}

		workers[rq].ok = 1;
		(nd->jobs_todo)--;
		pthread_mutex_unlock(workers[rq].mu);
	}
	stop_workers(nd, workers, &workers_alive);

	if(!(state_save(conf, nd->map, conf->jobs - nd->jobs_todo))) {
		fprintf(stderr, "Error while saving state: %s\n", strerror(errno));
		goto tidyup;
	}

	rv = render(conf, nd->map) ? 0 : 1;

tidyup:
	stop_workers(nd, workers, &workers_alive);

	if(nd) {
		nebula_data_destroy(nd);
	}

	if(workers) {
		for(i = 0; i < conf->threads; i++) {
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
