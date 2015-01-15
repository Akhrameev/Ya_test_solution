//
//  main.c
//  MaxOccursInArray
//
//  Created by Pavel Akhrameev on 16.01.15.
//  Copyright (c) 2015 Pavel Akhrameev. All rights reserved.
//

#include <stdio.h>      //tmp
#include <pthread.h>

#define NTHREADS 2

void *hola(void * arg)
{
    int myid=*(int *) arg;
    printf("Hello, world, I'm %d\n",myid);
    return arg;
}

int main(int argc,char *argv[])
{
    int worker;
    pthread_t threads[NTHREADS];                /* holds thread info */
    int ids[NTHREADS];                          /* holds thread args */
    int errcode;                                /* holds pthread error code */
    int *status;                                /* holds return code */
    /* create the threads */
    for (worker=0; worker<NTHREADS; worker++) {
    ids[worker]=worker;
    if (errcode=pthread_create(&threads[worker],/* thread struct             */
                               NULL,                    /* default thread attributes */
                               hola,                    /* start routine             */
                               &ids[worker])) {         /* arg to routine            */
        fprintf(stderr,"pthread_create: %d\n",errcode);
        exit(1);
        //errexit(errcode,"pthread_create");
        }
    }
    /* reap the threads as they exit */
    for (worker=0; worker<NTHREADS; worker++) {
        /* wait for thread to terminate */
        if (errcode=pthread_join(threads[worker],(void *) &status)) {
            //errexit(errcode,"pthread_join");
            fprintf(stderr,"pthread_join: %d\n",errcode);
            exit(1);
        }
        /* check thread's exit status and release its resources */
        if (*status != worker) {
            fprintf(stderr,"thread %d terminated abnormally\n",worker);
            exit(1);
        }
    }
    return(0);
}
