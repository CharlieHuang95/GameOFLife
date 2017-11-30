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
#define NUM_THREADS 8
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

inline char board_sum(char* board, int i, int jwest, int j, int jeast) {
    return board[j + i - 1] + board[j + i + 1] +
           board[jwest + i - 1] + board[jwest + i] + board[jwest + i + 1] +
           board[jeast + i - 1] + board[jeast + i] + board[jeast + i + 1];
}

void* modify_columns(void *arguments) {
    struct ThreadParameters* thread_params = arguments;
    int i, j;
    int inorth, isouth, jwest, jeast;
    int neighbor_count;
    int LDA = thread_params->ncols;
    char* temp;
    char* inboard = thread_params->inboard;
    char* outboard = thread_params->outboard;
    const int ncols = thread_params->ncols;
    const int nrows = thread_params->nrows;
    const int start = thread_params->start;
    const int end = thread_params->end;
    for (int iter = 0; iter < thread_params->num_iterations; iter++) {
        for (j = start; j < end; j++) {
            jwest = j-1;
            jeast = j+1;
            if (j == 0) { jwest = ncols - 1; }
            else if (j == ncols - 1) { jeast = 0; }

            // inorth = nrows - 1, i = 0, isouth = 1
            
            neighbor_count = BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, j) + BOARD (inboard, nrows - 1, jeast) +  
                BOARD (inboard, 0, jwest) + BOARD (inboard, 0, jeast) + BOARD (inboard, 1, jwest) + BOARD (inboard, 1, j) + 
                BOARD (inboard, 1, jeast);
            BOARD(outboard, 0, j) = (neighbor_count == 3) || (neighbor_count == 2 && BOARD (inboard, 0, j));
            int offset = jwest * ncols;
            for (i = 1; i < nrows - 1; i++) {
//                inorth = i-1;
//                isouth = i+1;
/*                neighbor_count = 
                    BOARD (inboard, inorth, jwest) + 
                    BOARD (inboard, inorth, j) + 
                    BOARD (inboard, inorth, jeast) + 
                    BOARD (inboard, i, jwest) +
                    BOARD (inboard, i, jeast) + 
                    BOARD (inboard, isouth, jwest) +
                    BOARD (inboard, isouth, j) + 
                    BOARD (inboard, isouth, jeast);
*/                neighbor_count = (inboard[offset] + inboard[jwest*ncols + i] + inboard[jwest*ncols + i + 1]) +
                                   (inboard[j*ncols + i - 1] + inboard[j*ncols + i + 1]) +
                                   (inboard[jeast*ncols + i - 1] + inboard[jeast*ncols + i] + inboard[jeast*ncols + i + 1]);
                //board_sum(inboard, i, jwest * ncols, j * ncols, jeast * ncols);
                outboard[ncols * j + i] = neighbor_count == 3 || (neighbor_count == 2 && inboard[ncols * j + i]); 
            }

            neighbor_count = BOARD (inboard, nrows - 2, jwest) + BOARD (inboard, nrows - 2, j) + BOARD (inboard, nrows - 2, jeast) +  
                BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, jeast) + BOARD (inboard, 0, jwest) + BOARD (inboard, 0, j) + 
                BOARD (inboard, 0, jeast);
            BOARD(outboard, nrows - 1, j) = (neighbor_count == 3) || (neighbor_count == 2 && BOARD (inboard, nrows - 1, j));

        }
        if (DEBUG) {
            printf("Wait\n");
        }
        temp = inboard;
        inboard = outboard;
        outboard = temp;
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


