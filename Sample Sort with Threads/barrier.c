// Author: Nat Tuck
// CS3650 starter code

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "barrier.h"

barrier*
make_barrier(int nn)
{
    barrier* bb = malloc(sizeof(barrier));

    bb->count = nn;
    bb->seen  = 0;

    pthread_mutex_init(&(bb->mutex), 0);
    pthread_cond_init(&(bb->cond), 0);

    return bb;
}

void
barrier_wait(barrier* bb)
{
    pthread_mutex_lock(&(bb->mutex));
    bb->seen++;

    if (bb->seen >= bb->count)
    {
        pthread_cond_broadcast(&(bb->cond));
    }

    while (bb->seen < bb->count) {
        pthread_cond_wait(&(bb->cond), &(bb->mutex));
    }

    pthread_mutex_unlock(&(bb->mutex));
}

void
free_barrier(barrier* bb)
{
    free(bb);
}

