//
//  main.c
//  MaxOccursInArray
//
//  Created by Pavel Akhrameev on 16.01.15.
//  Copyright (c) 2015 Pavel Akhrameev. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define NTHREADS 2

static const int numberOfAsciiChars = 256;
char maxOccuredCharInArray (char *array, int size);

#define TEST 1

#if (!TEST)
int main(int argc,char *argv[]) {
    char *array = "abccejbfwejhfbawhefbajwebfahwbefjhab)wejhfcba";
    char maxOccuredChar = maxOccuredCharInArray (array, (int)strlen(array));
    printf("<%c>", maxOccuredChar);
    return 0;
}
#else
#import <assert.h>
int main(int argc,char *argv[]) {
    //Не тестирую исключительные ситуации: неположительный размер массива, размер, указанный больше, чем строка - они вызовут exit(errorCode);
    char *array = "a";
    char maxOccuredChar = maxOccuredCharInArray (array, (int)strlen(array));
    assert(maxOccuredChar == 'a');
    array = "abba";
    maxOccuredChar = maxOccuredCharInArray (array, (int)strlen(array));
    assert(maxOccuredChar == 'a');
    //проверю сос трокой "abb" - для этого нужно отсечь чуть-чуть длину
    maxOccuredChar = maxOccuredCharInArray (array, (int)strlen(array) - 1);
    assert(maxOccuredChar == 'b');
    unsigned int maxPossibleLength = -1;    //здесь будет максимальный unsigned int
    //если мы его поделим пополам, у нас получится как раз максимальный int
    maxPossibleLength = maxPossibleLength/2;
    //можно было через sizeof аналогично пытаться просчитать
    const size_t length = (size_t)maxPossibleLength;
    array = malloc(length * sizeof(char));
    //пока в массиве просто кусок памяти - с какими значениями - не ясно
    for (size_t i = 0; i < length; ++i) {
        if (i == 2147483645) {
            i = i;
        }
        //если size_t совпадет по размеру с int, беды не будет (так как length половина от беззнакового)
        if (!(i%2)) {
            array[i] = 't';
        }
    }
    maxOccuredChar = maxOccuredCharInArray (array, (int)length);
    assert(maxOccuredChar == 't');
    //делаю равномерное распределение символов по массиву (если забью нулями - ноль будет преобладать)
    for (size_t i = 0; i < length; ++i) {
        array[i] = i % numberOfAsciiChars;
    }
    //в начало вписываю символы 'a', 'a' и 'b' - теперь они будут преобладать
    array[0] = 'a';
    array[1] = 'a';
    array[2] = 'b';
    maxOccuredChar = maxOccuredCharInArray (array, (int)length);
    assert(maxOccuredChar == 'a');
    //проверяю массив без 1 символа - тоже должна преобладать 'a'
    maxOccuredChar = maxOccuredCharInArray (array + sizeof(char) * 1, (int)length - 1);
    assert(maxOccuredChar == 'a');
    maxOccuredChar = maxOccuredCharInArray (array + sizeof(char) * 2, (int)length - 2);
    assert(maxOccuredChar == 'b');
    free(array);
    printf("<%c>", maxOccuredChar);
    return 0;
}
#endif

struct ArrayPointerAndSize {
    char *array;
    size_t size;
};

//размер передан в int, таким образом, int'а будет достаточно для набора статистики
int *maxOccuredCharInArrayInThread (char *array, int size);

typedef int     *(*returningIntArrayFunc)(void *);
typedef void    *(*returningVoidStarFunc)(void *);

int *maxOccuredCharInArrayInThreadWithOneArg(void *arg) {
    struct ArrayPointerAndSize str= *((struct ArrayPointerAndSize *) arg);
    return maxOccuredCharInArrayInThread(str.array, (int)str.size);
}

int *maxOccuredCharInArrayInThread (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        exit(0); //0 я не собираюсь разыменовывать!
    }
    int *charStat = (int *) calloc((size_t) numberOfAsciiChars, (size_t) sizeof(int));
    for (size_t i = 0; i < size; ++i) {
        char c = array[i];
        ++charStat[c];
    }
    return charStat;
}

char maxOccuredCharInArray (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        exit(1);
    }
    if (size < 2) { //оптимизация очевидных случаев
        return array[0];    //если элементов в массиве 1 или 2, то первый элемент гарантированно попадает в ответ
    }
    static const size_t maxDirectSize = 1024;
    char solution = 0;
    if (size < maxDirectSize) {
        int *charStat = (int *) calloc((size_t) numberOfAsciiChars, (size_t) sizeof(int));
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
        pthread_t threads[NTHREADS];                //информация потоков
        struct ArrayPointerAndSize *args;           //аргументы потоков
        int **resultStatistics;                     //результаты работы потоков
        resultStatistics = calloc(NTHREADS, sizeof(int *));
        args = (struct ArrayPointerAndSize *)calloc(NTHREADS, sizeof(struct ArrayPointerAndSize));
        size_t devidedSize = size / NTHREADS;
        size_t moduloOfDevidedSize = size % NTHREADS;
        //разделяю массив на NTHREADS частей.
        //первые moduloOfDevidedSize частей (например, 0) будут в себе хранить devidedSize+1 элементов
        //остальные - devidedSize элементов
        for (size_t i = 0; i < NTHREADS; ++i) {
            static size_t offset = 0;
            args[i].size = (i < moduloOfDevidedSize) ? devidedSize + 1 : devidedSize;
            args[i].array = array + sizeof(char) * offset;
            offset += args[i].size;
        }
        //мне показалось, что разделить этапы важнее, чем получить 1 цикл из NTHREADS==2 операций вместо двух циклов с тем же количеством итераций
        for (size_t i = 0; i < NTHREADS; ++i) {
            int errcode = pthread_create(&threads[i], NULL, (returningVoidStarFunc)maxOccuredCharInArrayInThreadWithOneArg, &args[i]);
            if (errcode) {
                fprintf(stderr,"pthread_create: %d\n",errcode);
                exit(3);
            }
        }
        for (size_t i = 0; i < NTHREADS; ++i) {
            int errcode = pthread_join(threads[i], (void *)&resultStatistics[i]);
            if (errcode) {
                fprintf(stderr,"pthread_join: %d\n",errcode);
                exit(4);
            }
            if (!resultStatistics[i]) {
                fprintf(stderr, "pthread %zu finished badly\n", i);
                exit(5);
            }
        }
        int maxOccured = 0;
        for (unsigned char i = 0; ; ++i) {
            //можно попробовать и здесь сделать досрочный выход - но это уже точно экономия "на спичках", так как этот цикл не может идти больше 256 итераций
            int currentCharOccured = 0;
            for (size_t j = 0; j < NTHREADS; ++j) {//цикл практически наверняка развернется компилятором
                currentCharOccured += resultStatistics[j][i];
            }
            if (currentCharOccured > maxOccured) {
                maxOccured = currentCharOccured;
                solution = (char)i;
            }
            if (i == numberOfAsciiChars-1) {
                break;
            }
        }
        for (size_t i = 0; i < NTHREADS; ++i) {
            free (resultStatistics[i]); //очищаю массивы, которые вернулись потоками
        }
        free(args);
        free(resultStatistics);
    }
    return solution;
}
