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

#define __UP_NTHREADS 2
#define __UP_MAX_DIRECT_SIZE 1024
#define __UP_NUMBER_OF_ASCII_CHARS 256

char mostFrequentCharacter (char *array, int size);

#define TEST 0

#if (!TEST)
int main(int argc,char *argv[]) {
    char *array = "abccejbfwejhfbawhefbajwebfahwbefjhab)wejhfcba";
    char mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    printf("<%c>", mostFrequentChar);
    return 0;
}
#else
#import <assert.h>
int main(int argc,char *argv[]) {
    //Не тестирую исключительные ситуации: неположительный размер массива, размер, указанный больше, чем строка - они вызовут exit(errorCode);
    char *array = "a";
    char mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    assert(mostFrequentChar == 'a');
    array = "abba";
    mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    assert(mostFrequentChar == 'a');
    //проверю сос трокой "abb" - для этого нужно отсечь чуть-чуть длину
    mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array) - 1);
    assert(mostFrequentChar == 'b');
    unsigned int maxPossibleLength = -1;    //здесь будет максимальный unsigned int
    //если мы его поделим пополам, у нас получится как раз максимальный int
    maxPossibleLength = maxPossibleLength/2;
    //можно было через sizeof аналогично пытаться просчитать
    const size_t length = (size_t)maxPossibleLength;
    array = malloc(length * sizeof(char));
    //пока в массиве просто кусок памяти - с какими значениями - не ясно
#if CHECK_MAJORITY
    for (size_t i = 0; i < length; ++i) {
        //если size_t совпадет по размеру с int, беды не будет (так как length половина от беззнакового)
        if (!(i%2)) {
            array[i] = 't';
        }
    }
    mostFrequentChar = mostFrequentCharacter (array, (int)length);
    assert(mostFrequentChar == 't');
#endif
    //делаю равномерное распределение символов по массиву (если забью нулями - ноль будет преобладать)
    for (size_t i = 0; i < length; ++i) {
        array[i] = (char)(i % __UP_NUMBER_OF_ASCII_CHARS);
    }
    //в начало вписываю символы 'a', 'a' и 'b' - теперь они будут преобладать
    for (size_t i = 0; i < 3; ++i) {
        if (!i || (i==1)) {
            array[i] = 'a';
        } else {
            array[i] = 'b';
        }
    }
    mostFrequentChar = mostFrequentCharacter (array, (int)length);
    assert(mostFrequentChar == 'a');
    //проверяю массив без 1 символа - тоже должна преобладать 'a'
    mostFrequentChar = mostFrequentCharacter (array + sizeof(char) * 1, (int)length - 1);
    assert(mostFrequentChar == 'a');
    mostFrequentChar = mostFrequentCharacter (array + sizeof(char) * 2, (int)length - 2);
    assert(mostFrequentChar == 'b');
    free(array);
    return 0;
}
#endif

struct ArrayPointerAndSize {
    char *array;
    size_t size;
};

//размер передан в int, таким образом, int'а будет достаточно для набора статистики
int *mostFrequentCharacterInThread (char *array, int size);

typedef int     *(*returningIntArrayFunc)(void *);
typedef void    *(*returningVoidStarFunc)(void *);

int *mostFrequentCharacterInThreadWithOneArg(void *arg) {
    struct ArrayPointerAndSize str= *((struct ArrayPointerAndSize *) arg);
    return mostFrequentCharacterInThread(str.array, (int)str.size);
}

int *mostFrequentCharacterInThread (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        exit(0); //0 я не собираюсь разыменовывать!
    }
    int *charStat = (int *) calloc((size_t) __UP_NUMBER_OF_ASCII_CHARS, (size_t) sizeof(int));
    for (size_t i = 0; i < size; ++i) {
        char c = array[i];
        ++charStat[c];
    }
    return charStat;
}

char mostFrequentCharacter (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        exit(1);
    }
    if (size < 2) { //оптимизация очевидных случаев
        return array[0];    //если элементов в массиве 1 или 2, то первый элемент гарантированно попадает в ответ
    }
    char solution = 0;
    if (size < __UP_MAX_DIRECT_SIZE) {
        int *charStat = (int *) calloc((size_t) __UP_NUMBER_OF_ASCII_CHARS, (size_t) sizeof(int));
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
            if (i == __UP_NUMBER_OF_ASCII_CHARS-1) {
                break;
            }
        }
        free(charStat);
    } else {
        pthread_t threads[__UP_NTHREADS];                //информация потоков
        struct ArrayPointerAndSize *args;           //аргументы потоков
        int **resultStatistics;                     //результаты работы потоков
        resultStatistics = calloc(__UP_NTHREADS, sizeof(int *));
        args = (struct ArrayPointerAndSize *)calloc(__UP_NTHREADS, sizeof(struct ArrayPointerAndSize));
        size_t devidedSize = size / __UP_NTHREADS;
        size_t moduloOfDevidedSize = size % __UP_NTHREADS;
        //разделяю массив на __UP_NTHREADS частей.
        //первые moduloOfDevidedSize частей (например, 0) будут в себе хранить devidedSize+1 элементов
        //остальные - devidedSize элементов
        size_t offset = 0;
        for (size_t i = 0; i < __UP_NTHREADS; ++i) {
            args[i].size = (i < moduloOfDevidedSize) ? devidedSize + 1 : devidedSize;
            args[i].array = array + sizeof(char) * offset;
            offset += args[i].size;
        }
        //мне показалось, что разделить этапы важнее, чем получить 1 цикл из __UP_NTHREADS==2 операций вместо двух циклов с тем же количеством итераций
        for (size_t i = 0; i < __UP_NTHREADS; ++i) {
            int errcode = pthread_create(&threads[i], NULL, (returningVoidStarFunc)mostFrequentCharacterInThreadWithOneArg, &args[i]);
            if (errcode) {
                fprintf(stderr,"pthread_create: %d\n",errcode);
                exit(3);
            }
        }
        for (size_t i = 0; i < __UP_NTHREADS; ++i) {
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
            for (size_t j = 0; j < __UP_NTHREADS; ++j) {//цикл практически наверняка развернется компилятором
                currentCharOccured += resultStatistics[j][i];
            }
            if (currentCharOccured > maxOccured) {
                maxOccured = currentCharOccured;
                solution = (char)i;
            }
            if (i == __UP_NUMBER_OF_ASCII_CHARS-1) {
                break;
            }
        }
        for (size_t i = 0; i < __UP_NTHREADS; ++i) {
            free (resultStatistics[i]); //очищаю массивы, которые вернулись потоками
        }
        free(args);
        free(resultStatistics);
    }
    return solution;
}
