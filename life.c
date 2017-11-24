/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"

#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])
#define NUM_THREADS 1

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

inline void modify_columns(int start, int end, char* outboard, char* inboard,
                           const int nrows, const int ncols) {
    int i, j;
    int inorth, isouth, jwest, jeast;
    char neighbor_count;
    const int LDA = ncols;
    for (j = start; j < end; j++) {
        jwest = j-1;
        jeast = j+1;
        if (j == 0) {
            jwest = ncols - 1;
        } else if (j == ncols - 1) {
            jeast = 0;
        }

        for (i = 0; i < nrows; i++) {
            inorth = i-1;
            isouth = i+1;
            if (i == 0) {
                inorth = nrows - 1;
            } else if (i == nrows - 1) {
                isouth = 0;
            }
            neighbor_count = 
                BOARD (inboard, inorth, jwest) + 
                BOARD (inboard, inorth, j) + 
                BOARD (inboard, inorth, jeast) + 
                BOARD (inboard, i, jwest) +
                BOARD (inboard, i, jeast) + 
                BOARD (inboard, isouth, jwest) +
                BOARD (inboard, isouth, j) + 
                BOARD (inboard, isouth, jeast);

            BOARD(outboard, i, j) = alivep (neighbor_count, BOARD (inboard, i, j));

        }
    }
}

char* game_of_life_parallel (char* outboard, char* inboard, const int nrows, 
                             const int ncols, const int gens_max) {
    /* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
    int curgen;
    for (curgen = 0; curgen < gens_max; curgen++) {
        /* HINT: you'll be parallelizing these loop(s) by doing a
           geometric decomposition of the output */
        modify_columns(0, ncols, outboard, inboard, nrows, ncols);

        SWAP_BOARDS( outboard, inboard );

    }
    /* 
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!! 
     */
    return inboard;
}


