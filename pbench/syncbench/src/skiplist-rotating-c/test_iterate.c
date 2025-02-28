/*
 * File:
 *   test.c
 * Author(s):
 *   Vincent Gramoli <vincent.gramoli@epfl.ch>
 * Description:
 *   Concurrent accesses to skip list integer set
 *
 * Copyright (c) 2009-2010.
 *
 * test.c is part of Synchrobench
 * 
 * Synchrobench is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include <atomic_ops.h>

//#include "tm.h"
//#include "lockfree.h"
#include "ptst.h"
#include "garbagecoll.h"

#define OPRATION
#define OFFSET 4

#define DEFAULT_DURATION                10000
#define DEFAULT_INITIAL                 256
#define DEFAULT_NB_THREADS              1
#define DEFAULT_RANGE                   0x7FFFFFFF
#define DEFAULT_SEED                    0
#define DEFAULT_UPDATE                  20
#define DEFAULT_ELASTICITY              4
#define DEFAULT_ALTERNATE               0
#define DEFAULT_EFFECTIVE               1
#define DEFAULT_UNBALANCED              0
#define DEFAULT_OPERATIONS              100

#define XSTR(s)                         STR(s)
#define STR(s)                          #s

#define ATOMIC_CAS_MB(a, e, v)          (DIMP_compare_and_swap_full((volatile AO_t *)(a), (AO_t)(e), (AO_t)(v)))
#define ATOMIC_FETCH_AND_INC_FULL(a)    (DIMP_fetch_and_add1_full((volatile AO_t *)(a)))

#define TRANSACTIONAL                   d->unit_tx

#define VAL_MIN                         INT_MIN
#define VAL_MAX                         INT_MAX

inline long rand_range(long r); /* declared in test.c */

#include "intset.h"
#include "background.h"

volatile AO_t stop;
unsigned int global_seed;
#ifdef TLS
__thread unsigned int *rng_seed;
#else /* ! TLS */
pthread_key_t rng_seed_key;
#endif /* ! TLS */
unsigned int levelmax;
/*
typedef struct barrier {
	pthread_cond_t complete;
	pthread_mutex_t mutex;
	int count;
	int crossing;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
	pthread_cond_init(&b->complete, NULL);
	pthread_mutex_init(&b->mutex, NULL);
	b->count = n;
	b->crossing = 0;
}

void barrier_cross(barrier_t *b)
{
	pthread_mutex_lock(&b->mutex);
	// One more thread through
	b->crossing++;
	// If not all here, wait 
	if (b->crossing < b->count) {
		pthread_cond_wait(&b->complete, &b->mutex);
	} else {
		pthread_cond_broadcast(&b->complete);
		// Reset for next time
		b->crossing = 0;
	}
	pthread_mutex_unlock(&b->mutex);
}
*/

barrier_t barrier, barrier_global;

int floor_log_2(unsigned int n) {
  int pos = 0;
  if (n >= 1<<16) { n >>= 16; pos += 16; }
  if (n >= 1<< 8) { n >>=  8; pos +=  8; }
  if (n >= 1<< 4) { n >>=  4; pos +=  4; }
  if (n >= 1<< 2) { n >>=  2; pos +=  2; }
  if (n >= 1<< 1) {           pos +=  1; }
  return ((n == 0) ? (-1) : pos);
}



/* 
 * Returns a pseudo-random value in [1;range).
 * Depending on the symbolic constant RAND_MAX>=32767 defined in stdlib.h,
 * the granularity of rand() could be lower-bounded by the 32767^th which might 
 * be too high for given values of range and initial.
 *
 * Note: this is not thread-safe and will introduce futex locks
 */
inline long rand_range(long r) {
	int m = RAND_MAX;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand()/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range(long r);

/* Thread-safe, re-entrant version of rand_range(r) */
inline long rand_range_re(unsigned int *seed, long r) {
	int m = RAND_MAX;
	int d, v = 0;
	
	do {
		d = (m > r ? r : m);		
		v += 1 + (int)(d * ((double)rand_r(seed)/((double)(m)+1.0)));
		r -= m;
	} while (r > 0);
	return v;
}
long rand_range_re(unsigned int *seed, long r);

typedef struct thread_data {
	unsigned int first;
	long range;
	int update;
	int unit_tx;
	int alternate;
	int effective;
	int operations;
	unsigned long nb_add;
	unsigned long nb_added;
	unsigned long nb_remove;
	unsigned long nb_removed;
	unsigned long nb_contains;
	unsigned long nb_found;
	unsigned long nb_aborts;
	unsigned long nb_aborts_locked_read;
	unsigned long nb_aborts_locked_write;
	unsigned long nb_aborts_validate_read;
	unsigned long nb_aborts_validate_write;
	unsigned long nb_aborts_validate_commit;
	unsigned long nb_aborts_invalid_memory;
	unsigned long nb_aborts_double_write;
	unsigned long max_retries;
	unsigned int seed;
	set_t *set;
	barrier_t *barrier;
	unsigned long failures_because_contention;
} thread_data_t;


void print_skiplist(set_t *set) {
	node_t *curr;
	int i, j;
	int arr[levelmax];

	for (i=0; i< sizeof arr/sizeof arr[0]; i++) arr[i] = 0;

	curr = set->head;
	do {
		printf("%lu", curr->key);
		for (i=0; i<curr->level-1; i++) {
			printf("-*");
		}
		arr[curr->level-1]++;
		printf("\n");
		curr = curr->next;
	} while (curr);
	for (j=0; j<MAX_LEVELS-1; j++)
		printf("%d nodes of level %d\n", arr[j], j);
}


void *test(void *data) {
	int unext, last = -1;
	unsigned int val = 0;

	thread_data_t *d = (thread_data_t *)data;

	/* Create transaction */
	TM_THREAD_ENTER();
	/* Wait on barrier */
	barrier_cross(d->barrier);

	/* Is the first op an update? */
	unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
/*
#ifdef ICC
	while (stop == 0) {
#else
        while (stop == 0) {
                BARRIER();
#endif
*/
	for (int i=0; i<d->operations;i++){

		if (unext) { // update

			if (last < 0) { // add

				val = rand_range_re(&d->seed, d->range);
				if (sl_add_old(d->set, val, TRANSACTIONAL)) {
					d->nb_added++;
					last = val;
				}
				d->nb_add++;

				//New Logic: n:1 inserts and deletes. val+M : to avoid comeplete randomness.
	      		#ifdef OPRATION
	      			val = (val + OFFSET) % d->range;
					if (sl_add_old(d->set, val, TRANSACTIONAL)) {
						d->nb_added++;
						last = val;
					}
					d->nb_add++;
	      		#endif

			} else { // remove

				if (d->alternate) { // alternate mode (default)
					if (sl_remove_old(d->set, last, TRANSACTIONAL)) {
						d->nb_removed++;
					}
					last = -1;
				} else {
					/* Random computation only in non-alternated cases */
					val = rand_range_re(&d->seed, d->range);
					/* Remove one random value */
					if (sl_remove_old(d->set, val, TRANSACTIONAL)) {
						d->nb_removed++;
						/* Repeat until successful, to avoid size variations */
						//last = -1;
					}
					last = -1;
				}
				d->nb_remove++;
			}

		} else { // read

			if (d->alternate) {
				if (d->update == 0) {
					if (last < 0) {
						val = d->first;
						last = val;
					} else { // last >= 0
						val = rand_range_re(&d->seed, d->range);
						last = -1;
					}
				} else { // update != 0
					if (last < 0) {
						val = rand_range_re(&d->seed, d->range);
						//last = val;
					} else {
						val = last;
					}
				}
			}	else val = rand_range_re(&d->seed, d->range);

			if (sl_contains_old(d->set, val, TRANSACTIONAL))
				d->nb_found++;
			d->nb_contains++;

		}

		/* Is the next op an update? */
		if (d->effective) { // a failed remove/add is a read-only tx
			unext = ((100 * (d->nb_added + d->nb_removed))
							 < (d->update * (d->nb_add + d->nb_remove + d->nb_contains)));
		} else { // remove/add (even failed) is considered as an update
			unext = (rand_range_re(&d->seed, 100) - 1 < d->update);
		}

#ifdef ICC
	}
#else
	}
#endif /* ICC */

	/* Free transaction */
	TM_THREAD_EXIT();

	return NULL;
}

void catcher(int sig)
{
	printf("CAUGHT SIGNAL %d\n", sig);
}

int main(int argc, char **argv)
{
	struct option long_options[] = {
		// These options don't set a flag
		{"help",                      no_argument,       NULL, 'h'},
		{"duration",                  required_argument, NULL, 'd'},
		{"initial-size",              required_argument, NULL, 'i'},
		{"thread-num",                required_argument, NULL, 't'},
		{"range",                     required_argument, NULL, 'r'},
		{"seed",                      required_argument, NULL, 'S'},
		{"update-rate",               required_argument, NULL, 'u'},
		{"unbalance",                 required_argument, NULL, 'U'},
		{"elasticity",                required_argument, NULL, 'x'},
		{"operations",                required_argument, NULL, 'o'},
		{NULL, 0, NULL, 0}
	};

	set_t *set;
	int i, c, size;
	unsigned int last = 0;
	unsigned int val = 0;
	unsigned long reads, effreads, updates, effupds, aborts, aborts_locked_read, aborts_locked_write,
	aborts_validate_read, aborts_validate_write, aborts_validate_commit,
	aborts_invalid_memory, aborts_double_write, max_retries, failures_because_contention;
	thread_data_t *data;
	pthread_t *threads;
	pthread_attr_t attr;
	//barrier_t barrier;
	struct timeval start, end;
	struct timespec timeout;
	int duration = DEFAULT_DURATION;
	int initial = DEFAULT_INITIAL;
	int nb_threads = DEFAULT_NB_THREADS;
	long range = DEFAULT_RANGE;
	int seed = DEFAULT_SEED;
	int update = DEFAULT_UPDATE;
	int unit_tx = DEFAULT_ELASTICITY;
	int alternate = DEFAULT_ALTERNATE;
	int effective = DEFAULT_EFFECTIVE;
	int operations = DEFAULT_OPERATIONS;
	int x =0;
	sigset_t block_set;
        unsigned long top;
        node_t *node = NULL;
				int unbalanced = DEFAULT_UNBALANCED;
	
	// By default, do not use mono int
  int mono_int = 0;
  // By default do not use reverse int also
  int reverse_int = 0;

	while(1) {
		i = 0;
		c = getopt_long(argc, argv, "hAmvf:d:i:t:r:S:u:U:x:o:", long_options, &i);

		if(c == -1)
			break;

		if(c == 0 && long_options[i].flag == 0)
			c = long_options[i].val;

		switch(c) {
				case 0:
					break;
				case 'h':
					printf("intset -- STM stress test "
								 "(skip list)\n"
								 "\n"
								 "Usage:\n"
								 "  intset [options...]\n"
								 "\n"
								 "Options:\n"
								 "  -h, --help\n"
								 "        Print this message\n"
								 "  -A, --Alternate\n"
								 "        Consecutive insert/remove target the same value\n"
								 "  -f, --effective <int>\n"
								 "        update txs must effectively write (0=trial, 1=effective, default=" XSTR(DEFAULT_EFFECTIVE) ")\n"
								 "  -d, --duration <int>\n"
								 "        Test duration in milliseconds (0=infinite, default=" XSTR(DEFAULT_DURATION) ")\n"
								 "  -i, --initial-size <int>\n"
								 "        Number of elements to insert before test (default=" XSTR(DEFAULT_INITIAL) ")\n"
								 "  -t, --num-threads <int>\n"
								 "        Number of threads (default=" XSTR(DEFAULT_NB_THREADS) ")\n"
								 "  -r, --range <int>\n"
								 "        Range of integer values inserted in set (default=" XSTR(DEFAULT_RANGE) ")\n"
								 "  -s, --seed <int>\n"
								 "        RNG seed (0=time-based, default=" XSTR(DEFAULT_SEED) ")\n"
								 "  -u, --update-rate <int>\n"
								 "        Percentage of update transactions (default=" XSTR(DEFAULT_UPDATE) ")\n"
								 "  -m, --mono-int\n"
                 "        Monotonically increasing integer values, beginning from 0\n"
                 "  -v, --reverse-int\n"
                 "        Reverse integers (i.e. from maximum to zero, monotonically decreasing)\n"
					       );
					exit(0);
				case 'A':
					alternate = 1;
					break;
				case 'm':
          			mono_int = 1;
          			break;
        		case 'v':
          			reverse_int = 1;
          			break;
				case 'f':
					effective = atoi(optarg);
					break;
				case 'd':
					duration = atoi(optarg);
					break;
				case 'i':
					initial = atoi(optarg);
					break;
				case 't':
					nb_threads = atoi(optarg);
					break;
				case 'r':
					range = atol(optarg);
					break;
				case 'S':
					seed = atoi(optarg);
					break;
				case 'u':
					update = atoi(optarg);
					break;
				case 'U':
                    unbalanced = atoi(optarg);
                    break;
                case 'x':
                    //unbalanced = atoi(optarg);
                    unit_tx = atoi(optarg);
                    break;
                case 'o':
                    //unbalanced = atoi(optarg);
                    operations = atoi(optarg);
                    break;
				case '?':
					printf("Use -h or --help for help\n");
					exit(0);
				default:
					exit(1);
		}
	}

	// Only one of these two can be valid
  if(reverse_int == 1 && mono_int == 1) {
    printf("ERROR: Can only choose one from -v and -m\n");
    exit(1);
  }

	assert(duration >= 0);
	assert(initial >= 0);
	assert(nb_threads > 0);
	assert(range > 0 && range >= initial);
	assert(update >= 0 && update <= 100);

	printf("Set type     : skip list\n");
	printf("Duration     : %d\n", duration);
	printf("Initial size : %u\n", initial);
	printf("Nb threads   : %d\n", nb_threads);
	printf("Value range  : %ld\n", range);
	printf("Seed         : %d\n", seed);
	printf("Update rate  : %d\n", update);
	printf("Elasticity   : %d\n", unit_tx);
	printf("Alternate    : %d\n", alternate);
	printf("Efffective   : %d\n", effective);
	printf("Operations   : %d\n", operations);
	printf("Mono int     : %d\n", mono_int);
  printf("Reverse int  : %d\n", reverse_int);
	printf("Type sizes   : int=%d/long=%d/ptr=%d/word=%d\n",
				 (int)sizeof(int),
				 (int)sizeof(long),
				 (int)sizeof(void *),
				 (int)sizeof(uintptr_t));

	timeout.tv_sec = duration / 1000;
	timeout.tv_nsec = (duration % 1000) * 1000000;

	if ((data = (thread_data_t *)malloc(nb_threads * sizeof(thread_data_t))) == NULL) {
		perror("malloc");
		exit(1);
	}
	if ((threads = (pthread_t *)malloc(nb_threads * sizeof(pthread_t))) == NULL) {
		perror("malloc");
		exit(1);
	}

	if (seed == 0)
		srand((int)time(0));
	else
		srand(seed);

	levelmax = floor_log_2((unsigned int) initial);

        /* create the skip list set and do inits */
        ptst_subsystem_init();
        gc_subsystem_init();
        set_subsystem_init();
        set = set_new(1);
	stop = 0;

        global_seed = rand();
#ifdef TLS
	rng_seed = &global_seed;
#else /* ! TLS */
	if (pthread_key_create(&rng_seed_key, NULL) != 0) {
		fprintf(stderr, "Error creating thread local\n");
		exit(1);
	}
	pthread_setspecific(rng_seed_key, &global_seed);
#endif /* ! TLS */

	// Init STM
	printf("Initializing STM\n");

	TM_STARTUP();

	// Populate set
	printf("Adding %d entries to set\n", initial);
	i = 0;

	while (i < initial) {
    if(mono_int) {
      val = i;
    } else if(reverse_int) {
      val = initial - 1 - i;
    } else {
      // Whether the key is unbalanced, if it is then just insert keys
      // in the given range (i.e. the number of iterations)
      if(unbalanced) {
        val = rand_range_re(&global_seed, initial);
      } else {
        val = rand_range_re(&global_seed, range);
      }
		}
		
		if (sl_add_old(set, val, 0)) {
			last = val;
			i++;
		}
	}
	size = set_size(set, 1);
	printf("Set size     : %d\n", size);
	printf("Level max    : %d\n", levelmax);

        // nullify all the index levels
        bg_stop();
        /*
        top = set->head->level-1;
        for (i = 0; i <= top; i++) {
                node_t *prev = set->head;
                node_t *node = prev->succs[IDX(i,sl_zero)];
                while (node) {
                        prev->succs[IDX(i,sl_zero)] = NULL;
                        prev->level = 0;
                        prev->raise_or_remove = 0;
                        prev = node;
                        node = node->succs[IDX(i, sl_zero)];
                }
        }
        */
        node = set->head;
        while (node) {
                int i;
                for (i = 0; i < MAX_LEVELS; i++)
                        node->succs[i] = NULL;
                node->level = 0;
                node->raise_or_remove = 0;
                node = node->next;
        }

        // wait till the list is balanced
        set->head->level = 1;
        bg_start(0);
        while (set->head->level < floor_log_2(initial)) {
            AO_nop_full();
        }
        bg_stop();
        bg_start(50000);
        printf("Number of levels is %lu\n", set->head->level);


	// Access set from all threads
	barrier_init(&barrier_global, nb_threads + 1);
	barrier_init(&barrier, nb_threads);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	for (i = 0; i < nb_threads; i++) {
		printf("Creating thread %d\n", i);
		data[i].first = last;
		data[i].range = range;
		data[i].update = update;
		data[i].unit_tx = unit_tx;
		data[i].alternate = alternate;
		data[i].effective = effective;
		data[i].operations = operations;
		data[i].nb_add = 0;
		data[i].nb_added = 0;
		data[i].nb_remove = 0;
		data[i].nb_removed = 0;
		data[i].nb_contains = 0;
		data[i].nb_found = 0;
		data[i].nb_aborts = 0;
		data[i].nb_aborts_locked_read = 0;
		data[i].nb_aborts_locked_write = 0;
		data[i].nb_aborts_validate_read = 0;
		data[i].nb_aborts_validate_write = 0;
		data[i].nb_aborts_validate_commit = 0;
		data[i].nb_aborts_invalid_memory = 0;
		data[i].nb_aborts_double_write = 0;
		data[i].max_retries = 0;
		data[i].seed = rand();
		data[i].set = set;
		data[i].barrier = &barrier;
		data[i].failures_because_contention = 0;
                if (pthread_create(&threads[i], &attr, test, (void *)(&data[i])) != 0) {
			fprintf(stderr, "Error creating thread\n");
			exit(1);
		}
	}
	pthread_attr_destroy(&attr);

	// Catch some signals
	if (signal(SIGHUP, catcher) == SIG_ERR ||
			//signal(SIGINT, catcher) == SIG_ERR ||
			signal(SIGTERM, catcher) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	// Start threads
	//barrier_cross(&barrier_global);

	printf("STARTING...\n");
	/*
	gettimeofday(&start, NULL);
	if (duration > 0) {
		nanosleep(&timeout, NULL);
	} else {
		sigemptyset(&block_set);
		sigsuspend(&block_set);
	}
	*/
/*
#ifdef ICC
	stop = 1;
#else
	AO_store_full(&stop, 1);
#endif */ /* ICC */

    stop = 1;

	gettimeofday(&end, NULL);
	printf("STOPPING...\n");

	// Wait for thread completion
	for (i = 0; i < nb_threads; i++) {
                if (pthread_join(threads[i], NULL) != 0) {
			fprintf(stderr, "Error waiting for thread completion\n");
			exit(1);
		}
	}

	duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 + start.tv_usec / 1000);
	aborts = 0;
	aborts_locked_read = 0;
	aborts_locked_write = 0;
	aborts_validate_read = 0;
	aborts_validate_write = 0;
	aborts_validate_commit = 0;
	aborts_invalid_memory = 0;
	aborts_double_write = 0;
	failures_because_contention = 0;
	reads = 0;
	effreads = 0;
	updates = 0;
	effupds = 0;
	max_retries = 0;
	for (i = 0; i < nb_threads; i++) {
                /*
		printf("Thread %d\n", i);
		printf("  #add        : %lu\n", data[i].nb_add);
		printf("    #added    : %lu\n", data[i].nb_added);
		printf("  #remove     : %lu\n", data[i].nb_remove);
		printf("    #removed  : %lu\n", data[i].nb_removed);
		printf("  #contains   : %lu\n", data[i].nb_contains);
		printf("  #found      : %lu\n", data[i].nb_found);
		printf("  #aborts     : %lu\n", data[i].nb_aborts);
		printf("    #lock-r   : %lu\n", data[i].nb_aborts_locked_read);
		printf("    #lock-w   : %lu\n", data[i].nb_aborts_locked_write);
		printf("    #val-r    : %lu\n", data[i].nb_aborts_validate_read);
		printf("    #val-w    : %lu\n", data[i].nb_aborts_validate_write);
		printf("    #val-c    : %lu\n", data[i].nb_aborts_validate_commit);
		printf("    #inv-mem  : %lu\n", data[i].nb_aborts_invalid_memory);
		printf("    #dup-w    : %lu\n", data[i].nb_aborts_double_write);
		printf("    #failures : %lu\n", data[i].failures_because_contention);
		printf("  Max retries : %lu\n", data[i].max_retries);
		*/
                aborts += data[i].nb_aborts;
		aborts_locked_read += data[i].nb_aborts_locked_read;
		aborts_locked_write += data[i].nb_aborts_locked_write;
		aborts_validate_read += data[i].nb_aborts_validate_read;
		aborts_validate_write += data[i].nb_aborts_validate_write;
		aborts_validate_commit += data[i].nb_aborts_validate_commit;
		aborts_invalid_memory += data[i].nb_aborts_invalid_memory;
		aborts_double_write += data[i].nb_aborts_double_write;
		failures_because_contention += data[i].failures_because_contention;
		reads += data[i].nb_contains;
		effreads += data[i].nb_contains +
		(data[i].nb_add - data[i].nb_added) +
		(data[i].nb_remove - data[i].nb_removed);
		updates += (data[i].nb_add + data[i].nb_remove);
		effupds += data[i].nb_removed + data[i].nb_added;
		size += data[i].nb_added - data[i].nb_removed;
		if (max_retries < data[i].max_retries)
			max_retries = data[i].max_retries;
	}
	printf("Set size      : %d (expected: %d)\n", set_size(set,1), size);
	printf("Duration      : %d (ms)\n", duration);
	printf("#txs          : %lu (%f / s)\n", reads + updates, (reads + updates) * 1000.0 / duration);

	printf("#read txs     : ");
	if (effective) {
		printf("%lu (%f / s)\n", effreads, effreads * 1000.0 / duration);
		printf("  #contains   : %lu (%f / s)\n", reads, reads * 1000.0 / duration);
	} else printf("%lu (%f / s)\n", reads, reads * 1000.0 / duration);

	printf("#eff. upd rate: %f \n", 100.0 * effupds / (effupds + effreads));

	printf("#update txs   : ");
	if (effective) {
		printf("%lu (%f / s)\n", effupds, effupds * 1000.0 / duration);
		printf("  #upd trials : %lu (%f / s)\n", updates, updates * 1000.0 /
					 duration);
	} else printf("%lu (%f / s)\n", updates, updates * 1000.0 / duration);

	printf("#aborts       : %lu (%f / s)\n", aborts, aborts * 1000.0 / duration);
	printf("  #lock-r     : %lu (%f / s)\n", aborts_locked_read, aborts_locked_read * 1000.0 / duration);
	printf("  #lock-w     : %lu (%f / s)\n", aborts_locked_write, aborts_locked_write * 1000.0 / duration);
	printf("  #val-r      : %lu (%f / s)\n", aborts_validate_read, aborts_validate_read * 1000.0 / duration);
	printf("  #val-w      : %lu (%f / s)\n", aborts_validate_write, aborts_validate_write * 1000.0 / duration);
	printf("  #val-c      : %lu (%f / s)\n", aborts_validate_commit, aborts_validate_commit * 1000.0 / duration);
	printf("  #inv-mem    : %lu (%f / s)\n", aborts_invalid_memory, aborts_invalid_memory * 1000.0 / duration);
	printf("  #dup-w      : %lu (%f / s)\n", aborts_double_write, aborts_double_write * 1000.0 / duration);
	printf("  #failures   : %lu\n",  failures_because_contention);
	printf("Max retries   : %lu\n", max_retries);

        bg_stop();
        bg_print_stats();

        //sl_set_print(set, 1);
        set_print_nodenums(set, 0);
        gc_subsystem_destroy();

	// Delete set
        set_delete(set);

	// Cleanup STM
	TM_SHUTDOWN();

#ifndef TLS
	pthread_key_delete(rng_seed_key);
#endif /* ! TLS */

	free(threads);
	free(data);

	return 0;
}

