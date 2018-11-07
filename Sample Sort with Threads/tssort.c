#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>

#include "float_vec.h"
#include "barrier.h"
#include "utils.h"

int
randomize(long max_value) {
    int rand = random();
    return (int)((float) rand / RAND_MAX * max_value);
}

int
comp_floats(const void * i, const void * j) {
    float new_i = * ((float * ) i);
    float new_j = * ((float * ) j);

    if (new_i < new_j) {
        return -1;
    } else if (new_i > new_j) {
        return 1;
    } else {
        return 0;
    }
}

void
qsort_floats(floats * xs) {
    qsort(xs-> data, xs-> size, sizeof(float), comp_floats);
}

typedef struct structure {

    int pnum;
    int P;
    long * sizes;

    floats * input;
    floats * samps;

    const char * output;
    barrier * bb;

}
sort_args;

floats*
sample(floats* input, int P)
{
    int counter = 3 * (P - 1);
    float samps[counter];
    floats* floats = make_floats(0);

    for (int ii = 0; ii < counter; ii++) 
    {
        samps[ii] = input->data[randomize(input->size)];
    }

    qsort(&samps, counter, sizeof(float), comp_floats);
    floats_push(floats, 0.0f);

    for (int ii = 0; ii < counter; ii += 3) 
    {
        floats_push(floats, samps[ii + 1]);
    }

    floats_push(floats, INFINITY);
    return floats;
}

void*
sort_worker(sort_args* args)
{
    int rv;
    int begin = 0;

    float a_s_v = args->samps->data[args->pnum];
    float a_s_v_inc = args->samps->data[args->pnum + 1];

    floats* floats = make_floats(0);

    for(int ii = 0; ii < args->input->size; ii++)
    {
        if(args->input->data[ii] >= a_s_v && args->input->data[ii] < a_s_v_inc)
        {
            floats_push(floats, args->input->data[ii]);
        }
    }

    args->sizes[args->pnum] = floats->size;
    printf("%d: start %.04f, count %ld\n", args->pnum, args->samps->data[args->pnum], args->sizes[args->pnum]);
    qsort_floats(floats);
    barrier_wait(args->bb);
    
    for(int ii = 0; ii < args->pnum; ii++)
    {
        begin = begin + args->sizes[ii];
    }

    int open_file = open(args->output, O_WRONLY, 0644);

    check_rv(open_file);
    rv = lseek(open_file, sizeof(long) + (begin*sizeof(float)), SEEK_SET);
    check_rv(rv);
    write(open_file, floats->data, (floats->size*sizeof(float)));
    rv = close(open_file);
    check_rv(rv);
    free_floats(floats);
    return 0;
}

void
run_sort_workers(floats * input,
    const char * output, int P, floats * samps, long * sizes, barrier * bb) {
    int rv;

    pthread_t threads[P];
    sort_args args[P];

    for (int ii = 0; ii < P; ii++) {

        args[ii].pnum = ii;
        args[ii].bb = bb;
        args[ii].input = input;
        args[ii].output = output;
        args[ii].P = P;
        args[ii].samps = samps;
        args[ii].sizes = sizes;

        rv = pthread_create( & (threads[ii]), 0, (void * ) sort_worker, & (args[ii]));
        assert(rv == 0);
    }

    for (int ii = 0; ii < P; ii++) {
        rv = pthread_join(threads[ii], 0);
        check_rv(rv);
    }

    return;
}

void
sample_sort(floats * input,
    const char * output, int P, long * sizes, barrier * bb) {
    floats * samps = sample(input, P);
    run_sort_workers(input, output, P, samps, sizes, bb);
    free_floats(samps);
}

int
main(int argc, char * argv[]) {
    alarm(120);

    if (argc != 4) {
        printf("Usage:\n");
        printf("\t%s P input.dat output.dat\n", argv[0]);
        return 1;
    }

    int rv;

    long counter;

    const int P = atoi(argv[1]);
    const char * iname = argv[2];
    const char * oname = argv[3];

    seed_rng();

    int open_file = open(iname, O_RDONLY, 0644);

    check_rv(open_file);
    rv = read(open_file, & counter, sizeof(long));
    check_rv(rv);
    rv = lseek(open_file, sizeof(long), SEEK_SET);
    check_rv(rv);

    float * f = malloc(counter * sizeof(float));

    rv = read(open_file, f, counter * sizeof(float));
    check_rv(rv);

    floats * floats = make_floats(0);

    for (int i = 0; i < counter; i++) {
        floats_push(floats, f[i]);
    }

    int ofd = open(oname, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    check_rv(ofd);
    rv = ftruncate(ofd, sizeof(counter) + (counter * sizeof(float)));
    check_rv(rv);
    write(ofd, & counter, sizeof(counter));
    rv = close(ofd);
    check_rv(rv);
    barrier * bb = make_barrier(P);

    long * sizes = malloc(P * sizeof(long));
    sample_sort(floats, oname, P, sizes, bb);

    free(sizes);
    free(f);

    free_barrier(bb);
    free_floats(floats);

    return 0;
}

