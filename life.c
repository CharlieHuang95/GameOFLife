/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include "pthread.h"
#include <stdlib.h>

#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])
#define NUM_THREADS 16
#define DEBUG 0

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/

char* game_of_life_parallel (char* outboard, char* inboard, const int nrows,
        const int ncols, const int gens_max);

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* outboard, 
	      char* inboard,
	      const int nrows,
	      const int ncols,
	      const int gens_max)
{
  return game_of_life_parallel (outboard, inboard, nrows, ncols, gens_max);
}

struct ThreadParameters {
    int start;
    int end;
    char* outboard;
    char* inboard;
    int nrows;
    int ncols;
    int num_iterations;
};

pthread_barrier_t iteration_barrier;

void* modify_columns(void *arguments) {
    struct ThreadParameters* thread_params = arguments;
    int i, j;
    int inorth, isouth, jwest, jeast;
    char neighbor_count;
    const int LDA = thread_params->ncols;
    for (int iter = 0; iter < thread_params->num_iterations; iter++) {
        for (j = thread_params->start; j < thread_params->end; j++) {
            jwest = j-1;
            jeast = j+1;
            if (j == 0) {
                jwest = thread_params->ncols - 1;
            } else if (j == thread_params->ncols - 1) {
                jeast = 0;
            }

            for (i = 0; i < thread_params->nrows; i++) {
                inorth = i-1;
                isouth = i+1;
                if (i == 0) {
                    inorth = thread_params->nrows - 1;
                } else if (i == thread_params->nrows - 1) {
                    isouth = 0;
                }
                neighbor_count = 
                    BOARD (thread_params->inboard, inorth, jwest) + 
                    BOARD (thread_params->inboard, inorth, j) + 
                    BOARD (thread_params->inboard, inorth, jeast) + 
                    BOARD (thread_params->inboard, i, jwest) +
                    BOARD (thread_params->inboard, i, jeast) + 
                    BOARD (thread_params->inboard, isouth, jwest) +
                    BOARD (thread_params->inboard, isouth, j) + 
                    BOARD (thread_params->inboard, isouth, jeast);

                BOARD(thread_params->outboard, i, j) =
                    alivep (neighbor_count, BOARD (thread_params->inboard, i, j));

            }
        }
        if (DEBUG) {
            printf("Wait\n");
        }
        char* temp = thread_params->inboard;
        thread_params->inboard = thread_params->outboard;
        thread_params->outboard = temp;
        pthread_barrier_wait(&iteration_barrier);
    }
    return NULL;
}

char* game_of_life_parallel (char* outboard, char* inboard, const int nrows, 
                             const int ncols, const int gens_max) {
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    if (DEBUG) {
        printf("START\n");
    }
    struct ThreadParameters thread_params[NUM_THREADS];
    pthread_t tids[NUM_THREADS];
    pthread_barrier_init(&iteration_barrier, NULL, NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_params[i].ncols = ncols;
        thread_params[i].nrows = nrows;
        thread_params[i].start = ncols / NUM_THREADS * i;
        thread_params[i].end = ncols / NUM_THREADS * (i + 1);
        thread_params[i].inboard = inboard;
        thread_params[i].outboard = outboard;
        thread_params[i].num_iterations = gens_max;
        pthread_create(&tids[i], NULL, &modify_columns, &thread_params[i]);
    }
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(tids[i], NULL);
    }
    /* 
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!! 
     */
    return inboard;
}


