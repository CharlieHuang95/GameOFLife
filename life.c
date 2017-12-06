/*****************************************************************************
 * life.c
 * 1)We parallelized GoL with pthreads and twp syncrhomization primitives 
 * supported by the pthread libirary.
 *      -1.1) pthread create and join. pthread join is called at the end of 
 *            the program, right before we return the output board
 *      -1.2) pthread barriers. At the end of each generation, each thread is
 *            blocked until all the threads finish that generation
 * 2)Workload is evenly distributed among threads. Wordload per thread is
 *   parametrized by the NUM_THREADS macro
 * 3)Did some hand code optimizations inside the "modify_columns" function:
 *      -3.1) created local variables to store the passed-in parameters.
 *      -3.2) changed alive_p function by modifying the logic by writing out 
                a Karnaugh Map to determine the least amount of logic required.   
 *      -3.3) Replaced the modulo instructions in the original sequential 
 *            implmentation.
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include "pthread.h"
#include <stdlib.h>

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])
#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define NUM_THREADS 8
#define DEBUG 0

/*****************************************************************************
 * Function Prototype: game_of_life_parallel 
 * game_of_life_parallel is called by game_of_life,
 * is a parallelized implementation.
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
    pthread_barrier_t* barrier;
};


/*****************************************************************************
 * Function definition: modify_columns
 * modify_columns looks at elements by rows then columns.
 * 1. Reduce memory references by creating local variabes for the parameters
 * pointed by the arguments pointer.
 * 2. Function inlining and reduction in strength: removed the alive_p function
 * call by modifying the logic by writing out a Karnaugh Map to determine
 * the least amount of logic required.
 * 3. Reduction in strength: It removed the modulo operation, which is in the 
 * original sequential implementation, by separating the two problematic rows 
 * (the first and last rows) from the inner loop.
 ****************************************************************************/
void* modify_columns(void *arguments) {
    struct ThreadParameters* thread_params = arguments;
    int i, j;
    int jwest, jeast;
    int neighbor_count;
    int LDA = thread_params->ncols;
    char* temp;
    char* inboard = thread_params->inboard;
    char* outboard = thread_params->outboard;
    const int ncols = thread_params->ncols;
    const int nrows = thread_params->nrows;
    const int start = thread_params->start;
    const int end = thread_params->end;
    pthread_barrier_t* barrier = thread_params->barrier;
    for (int iter = 0; iter < thread_params->num_iterations; iter++) {
        for (j = start; j < end; j++) {
            jwest = j-1;
            jeast = j+1;
            if (j == 0) { jwest = ncols - 1; }
            else if (j == ncols - 1) { jeast = 0; }
            
            neighbor_count = BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, j) + BOARD (inboard, nrows - 1, jeast) +  
                BOARD (inboard, 0, jwest) + BOARD (inboard, 0, jeast) + BOARD (inboard, 1, jwest) + BOARD (inboard, 1, j) + 
                BOARD (inboard, 1, jeast);
            BOARD(outboard, 0, j) = (neighbor_count == 3) || (neighbor_count == 2 && BOARD (inboard, 0, j));
            for (i = 1; i < nrows - 1; i++) {
                neighbor_count = (inboard[jwest*ncols + i - 1] + inboard[jwest*ncols + i] + inboard[jwest*ncols + i + 1]) +
                                   (inboard[j*ncols + i - 1] + inboard[j*ncols + i + 1]) +
                                   (inboard[jeast*ncols + i - 1] + inboard[jeast*ncols + i] + inboard[jeast*ncols + i + 1]);
                //board_sum(inboard, i, jwest * ncols, j * ncols, jeast * ncols);
                outboard[ncols * j + i] = (inboard[ncols * j + i] && neighbor_count == 2) || (neighbor_count == 3); 
            }

            neighbor_count = BOARD (inboard, nrows - 2, jwest) + BOARD (inboard, nrows - 2, j) + BOARD (inboard, nrows - 2, jeast) +  
                BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, jeast) + BOARD (inboard, 0, jwest) + BOARD (inboard, 0, j) + 
                BOARD (inboard, 0, jeast);
            BOARD(outboard, nrows - 1, j) = (neighbor_count == 3) || (neighbor_count == 2 && BOARD (inboard, nrows - 1, j));

        }
        temp = inboard;
        inboard = outboard;
        outboard = temp;
        pthread_barrier_wait(barrier);
    }
    return NULL;
}

/*****************************************************************************
 * Function definition: game_of_life_parallel 
 * game_of_life_parallel creates NUM_THREAD threads and
 * give an approximately even amount of workload to each thread. Workload
 * is divided evenly by columns.
 * Threads also joined in this function.
 * game_of_life_parallel returns the final board result to its caller
 ****************************************************************************/
char* game_of_life_parallel (char* outboard, char* inboard, const int nrows, 
                             const int ncols, const int gens_max) {
    struct ThreadParameters thread_params[NUM_THREADS];
    pthread_t tids[NUM_THREADS];
    pthread_barrier_t* iteration_barrier = malloc(sizeof(pthread_barrier_t));
    pthread_barrier_init(iteration_barrier, NULL, NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_params[i].ncols = ncols;
        thread_params[i].nrows = nrows;
        thread_params[i].start = ncols / NUM_THREADS * i;
        thread_params[i].end = ncols / NUM_THREADS * (i + 1);
        thread_params[i].inboard = inboard;
        thread_params[i].outboard = outboard;
        thread_params[i].num_iterations = gens_max;
        thread_params[i].barrier = iteration_barrier;
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


