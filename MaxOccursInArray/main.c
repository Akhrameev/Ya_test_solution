//
//  main.c
//  MaxOccursInArray
//
//  Created by Pavel Akhrameev on 16.01.15.
//  Copyright (c) 2015 Pavel Akhrameev. All rights reserved.
//

#include <stdio.h>      //tmp
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define NTHREADS 2

void *hola(void * arg)
{
    int myid=*(int *) arg;
    printf("Hello, world, I'm %d\n",myid);
    return arg;
}

#if (!HARD)

char maxOccuredCharInArray (char *array, int size);

int main(int argc,char *argv[]) {
    char *array = "abcccba";
    char maxOccuredChar = maxOccuredCharInArray (array, (int)strlen(array));
    printf("<%c>", maxOccuredChar);
    return 0;
}

char maxOccuredCharInArray (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        exit(1);
    }
    if (size < 2) { //оптимизация очевидных случаев
        return array[0];    //если элеменотов в массиве 1 или 2, то первый элемент гарантированно попадает в ответ
    }
    static const size_t maxDirectSize = 1024;
    static const int numberOfAsciiChars = 256;
    char solution = 0;
    if (size < maxDirectSize) {
        //alloc int array with zeros
        int *charStat = (int *) calloc((size_t) numberOfAsciiChars, (size_t) sizeof(int));
        //size is int, so int is enough for storing stats
        for (size_t i = 0; i < size; ++i) {
            char c = array[i];
            if ((i < size - 1) && (charStat[c] >= size/2+1)) {
                //оптимизация, чтобы не проходить весь массив
                free(charStat);
                solution = c;
                return solution;
            }
            ++charStat[c];
        }
        int maxOccured = 0;
        for (unsigned char i = 0; ; ++i) {
            int currentCharOccured = charStat[i];
            if (currentCharOccured > maxOccured) {
                maxOccured = currentCharOccured;
                solution = (char)i;
            }
            if (i == numberOfAsciiChars-1) {
                break;
            }
        }
        free(charStat);
    } else {
        
    }
    return solution;
}

#else

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

#endif
