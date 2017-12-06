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
 * 3)Did some hand code optimizations
 *      -3.1) Replaced the modulo instructions in the original sequential 
 *            implmentation.
 *      -3.2) Did loop-unrolling. Index increases by 2. Reduced overhead in the
 *            for loop.
 *      -3.3) Reduced memory access from 8 accesses/element to 
 *            6 accesses/2 elements = 3/elements.
 *            For details, look at modify_columns implementation.
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
 * modify_columns looks at elements by rows then columns. We have three
 * optimizations.
 * 1) Reduction in strength: It removed the modulo operation, which is in the 
 * original sequential implementation, by separating the two problematic rows 
 * (the first and last rows) from the inner loop.
 * 2) Loop-unrolling: index increments by 2. Reduced for loop overhead
 * 3) Reduction in cache accesses: from 8/element to 3/element by passing values
 * from registers to registers, instead of reading from memory between iterations
 ****************************************************************************/
void* modify_columns(void *arguments) {
    struct ThreadParameters* thread_params = arguments;
    int i, j;
    int inorth, isouth, isouth_south,jwest, jeast;
    char neighbor_count, abv_neighbor_count, blw_neighbor_count;
    const int LDA = thread_params->ncols;
    char* temp;
    char* inboard = thread_params->inboard;
    char* outboard = thread_params->outboard;
    const int ncols = thread_params->ncols;
    const int nrows = thread_params->nrows;
    const pthread_barrier_t* barrier = thread_params->barrier;
    for (int iter = 0; iter < thread_params->num_iterations; iter++) {
        for (j = thread_params->start; j < thread_params->end; j++) {
            jwest = j-1;
            jeast = j+1;
            if (j == 0) { jwest = ncols - 1; }
            else if (j == ncols - 1) { jeast = 0; }
            
            neighbor_count = BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, j) + BOARD (inboard, nrows - 1, jeast) +  
                BOARD (inboard, 0, jwest) + BOARD (inboard, 0, jeast) + BOARD (inboard, 1, jwest) + BOARD (inboard, 1, j) + 
                BOARD (inboard, 1, jeast);
            BOARD(outboard, 0, j) = alivep (neighbor_count, BOARD (inboard, 0, j));
            char ele_abv_0 = BOARD(inboard,0,jwest);
            char ele_abv_1 = BOARD(inboard,0,j);
            char ele_abv_2 = BOARD(inboard,0,jeast);
            char ele_0_0 = BOARD(inboard,1,jwest);
            char ele_0_1 = BOARD(inboard,1,j);
            char ele_0_2 = BOARD(inboard,1,jeast);
            for (i = 1; i < nrows - 2; i+=2) {
                inorth = i-1;
                isouth = i+1;
                isouth_south = i+2;
                char ele_1_0 = BOARD(inboard,isouth,jwest);
                char ele_1_1 = BOARD(inboard,isouth,j);
                char ele_1_2 = BOARD(inboard,isouth,jeast);
                char ele_blw_0 = BOARD(inboard,isouth_south,jwest);
                char ele_blw_1 = BOARD(inboard,isouth_south,j);
                char ele_blw_2 = BOARD(inboard,isouth_south,jeast);
                abv_neighbor_count = 
                    ele_abv_0 + 
                    ele_abv_1 + 
                    ele_abv_2 + 
                    ele_0_0 +
                    ele_0_2 + 
                    ele_1_0 +
                    ele_1_1 + 
                    ele_1_2;
                blw_neighbor_count = 
                    ele_0_0 + 
                    ele_0_1 +
                    ele_0_2 +
                    ele_1_0 + 
                    ele_1_2 +
                    ele_blw_0 +
                    ele_blw_1 +
                    ele_blw_2;

                BOARD(outboard, i, j) = ((abv_neighbor_count == (char) 3) && !ele_0_1) ||
                                        ((abv_neighbor_count >= 2) && (abv_neighbor_count <= 3) && ele_0_1);
                BOARD(outboard, i+1,j)= ((blw_neighbor_count == (char) 3) && !ele_1_1) ||
                                        ((blw_neighbor_count >= 2) && (blw_neighbor_count <= 3) && ele_1_1);
                ele_abv_0 = ele_1_0; ele_abv_1 = ele_1_1; ele_abv_2 = ele_1_2;
                ele_0_0 = ele_blw_0; ele_0_1 = ele_blw_1; ele_0_2 = ele_blw_2;
            }
            if (i!=nrows-1) {
                neighbor_count = BOARD(inboard,i-1,jwest) + BOARD(inboard,i-1,j) + BOARD(inboard,i-1,jeast) +
                    BOARD(inboard,i,jwest) + BOARD(inboard,i,jeast) + BOARD(inboard,i+1,jwest) + BOARD(inboard,i+1,j) + 
                    BOARD(inboard,i+1,jeast);
                BOARD(outboard, i, j) = ((neighbor_count == (char) 3) && !BOARD(inboard, i, j)) ||
                                        ((neighbor_count >= 2) && (neighbor_count <= 3) && BOARD(inboard, i, j)); 
            }
            neighbor_count = BOARD (inboard, nrows - 2, jwest) + BOARD (inboard, nrows - 2, j) + BOARD (inboard, nrows - 2, jeast) +  
                BOARD (inboard, nrows - 1, jwest) + BOARD (inboard, nrows - 1, jeast) + BOARD (inboard, 0, jwest) + BOARD (inboard, 0, j) + 
                BOARD (inboard, 0, jeast);
            BOARD(outboard, nrows - 1, j) = alivep (neighbor_count, BOARD (inboard, nrows - 1, j));

        }
        if (DEBUG) {
            printf("Wait\n");
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
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    if (DEBUG) {
        printf("START\n");
    }
    struct ThreadParameters thread_params[NUM_THREADS];
    pthread_t tids[NUM_THREADS];
    pthread_barrier_t iteration_barrier;
    pthread_barrier_init(&iteration_barrier, NULL, NUM_THREADS);
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_params[i].ncols = ncols;
        thread_params[i].nrows = nrows;
        thread_params[i].start = ncols / NUM_THREADS * i;
        thread_params[i].end = ncols / NUM_THREADS * (i + 1);
        thread_params[i].inboard = inboard;
        thread_params[i].outboard = outboard;
        thread_params[i].num_iterations = gens_max;
        thread_params[i].barrier = &iteration_barrier;
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


