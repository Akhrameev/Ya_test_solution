//
//  main.c
//  MaxOccursInArray
//
//  Created by Pavel Akhrameev on 16.01.15.
//  Copyright (c) 2015 Pavel Akhrameev. All rights reserved.
//

// Задача
/*
 Напишите реализацию для функции, которая принимает на вход последовательность ASCII-символов и
 выдаёт самый часто повторяющийся символ:
 
 char mostFrequentCharacter(char* str, int size);
 
 Функция должна быть оптимизирована для выполнения на устройстве с двухядерным ARM-процессором
 и бесконечным количеством памяти.
 */

//Рассуждения:
/*
 Сразу в голову приходит, что
 1. массив прийдется прочитать хотя бы раз (можно избежать это, если 1 символ занимает половину или больше половины массива)
 2. выделить память на 256 (количество ASCII-символов) int мы сможем (даже если будет sizeof(int)==16 нужно всего 4 Кб памяти)
 3. цикл по 256 элементам особого смысла оптимизировать нет (хотя, в целом, можно)
 
 То есть вариант 1 раз пройти по всему массиву и посчитать количество каждого символа по-отдельности в массиве мы можем - найти среди них первый максимум - тоже плёвое дело, решаемое циклом по массиву длины 256
 
 Но этот вариант не выглядит как-то оптимизированным под двухъядерный ARM.
 
 Количество ядер намекает на преимущества при использовании многопоточности.
 Но на маленьких массивах накладные расходы на создание потока будут превышать профит.
 Также мы теряем оптимизацию на чтение не полного массива (можем только оставить неполный проход по массиву длины 256 - но это, как я выше писал, я посчитал излишним).
 На массивах длины меньше 2 вообще, очевидно, ничего делать не надо.
 На массивах, содержащих небольшое количество символов, также можно выиграть в памяти, если использовать хеш-таблицу - но память, как выше сказано, у нас не ограничена. Не будем экономить эти несколько Кб.
 Реализовывать хранение до какой-то длины (меньше или равной 256) в хеш-такблице, а при превышении этого количества - в массиве я почитал накладным. 
 На массивах с большой длиной (и с маленьким количеством символов) также в накладные расходы попадет и функция вычисления хеша, что нам не на руку. А также это может сильно осложнить код.
 
 При многопоточности могут возникать проблемы при записи, параллельное чтение же нам доступно без проблем.
 Таким образом, входную строку (до 2Гб у меня на машине) мы не будем никуда копировать, а будем двумя потоками читать с исходного места.
 Писать будем в 2 отдельных массива (раз двухъядерный процессор), а потом просто в цикле сложим числа из массивов (к элементам массива доступ по указателю скор - нет смысла мудрить и помещать всю статистику в 1 массив, особенно выделять под него доп. память, хотя в отладке это могло бы помочь).
 
 Для решения я выбрал чистый Си (по синтаксису заголовка функции выбрал), что может быть нехорошо: в нем нет исключений. Могу либо через сигналы сообщать об ошибке, либо использовать errno или какую-то другую переменную под ошибку. Сейчас я использую просто exit с ненулевым кодом.
 
 Для того, чтобы убедиться, что мое решение верно решает задачу, я написал несколько тестов на случаи с корректными входными данными (чтобы не нарваться на exit).
 Несколько маленьких массивов разрешаются простым однопоточным алгоритмом, также я проверил граничные случаи с большим количеством элементов в массиве. У меня не было задачи добиться 100%-ого тестового покрытия. В коде есть только те тесты, которые я посчитал необходимыми. Замена maxPossibleLength перед установкой ее значения в length мной также производилась (в тестах не отражено).
 
 
 Итог: около 200 строк кода вместо 25-30 (случай с размером меньше __UP_MAX_DIRECT_SIZE)
 Такое решение сложно в поддержке и может быть оправдано только если оптимизации крайне необходимы.
 В продакшн я бы такой код не пустил много-много раз не подумав.
 
 P.S. статический анализатор Xcode предупреждений не выдал.
 
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define __UP_NTHREADS 2
#define __UP_MAX_DIRECT_SIZE 16384
#define __UP_NUMBER_OF_ASCII_CHARS 256

#include <signal.h>
#define __UP_SIGNAL_CODE SIGUSR1
//Этот метод может вернуть 0 в качестве символа и вызвать raise(__UP_SIGNAL_CODE).
char mostFrequentCharacter (char *array, int size);
//Функция возвращает для строк отрицательной длины - символ \0 и вызывает raise
//Для строк длины 1 и 2 возвращается первый символ строки
//Для строк большей длины возвращает первый (с точки зрения нумерации символов как беззнаковых) символ с максимальным числом вхождений
//Требования поставить проверку, чтобы среди символов с максимальным числом вхождений приоритетно был символ, отличный от \0, не было.
//Его можно добавить - мне кажется, это могло бы более интуитивно понятно. Но это добавит еще оперций в цикл по 256 элементам. Не критично, конечно.
//Также могло бы быть разумно добавить приоретизацию по коду и для строк длины 2 (то есть если будут идти символы \0 и \93), чтобы вернулся \93.
//Но такого требования также не было.

#define TEST 0

#if (!TEST)

static int errorFlag = 0;

static void hdl(int sig) {
    //Важно не забыть (lldb) pro hand -p true -s false <__UP_SIGNAL_CODE>
    //Здесь можно будет узнать об исключительной ситуации
    errorFlag = sig;
}

int main(int argc,char *argv[]) {
    //кусочек кода с http://habrahabr.ru/post/141206/ для обработки сигналов - думал, что я неверно что-то помню, оказалось, lldb перехватывал сигнал
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = hdl;
    sigset_t   set;
    sigemptyset(&set);
    sigaddset(&set, __UP_SIGNAL_CODE);
    act.sa_mask = set;
    sigaction(__UP_SIGNAL_CODE, &act, 0);
    char *array = "abccejbfwejhfbawhefbajwebfahwbefjhab)wejhfcba";
    errorFlag = 0;
    char mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    if (!mostFrequentChar && errorFlag) {
        fprintf(stderr, "Error occured during processing mostFrequentCharacter (or other circumstances for calling __UP_SIGNAL_CODE)");
    } else {
        printf("<%c>", mostFrequentChar);
    }
    return EXIT_SUCCESS;
}
#else
#include <assert.h>
#include <time.h>
int main(int argc,char *argv[]) {
    //Не тестирую исключительные ситуации: неположительный размер массива, размер, указанный больше, чем строка - они вызовут exit(errorCode);
    clock_t begin, end;
    double time_spent;
    
    begin = clock();
    char *array = "a";
    char mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    assert(mostFrequentChar == 'a');
    array = "abba";
    mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    assert(mostFrequentChar == 'a');
    //проверю со строкой "abb" - для этого нужно отсечь чуть-чуть длину
    mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array) - 1);
    assert(mostFrequentChar == 'b');
    //теперь проверю со строкой "bba" - должен выйти, не прочитав последний символ
    mostFrequentChar = mostFrequentCharacter (array + sizeof(char) * 1, (int)strlen(array) - 1);
    assert(mostFrequentChar == 'b');
    array = "abbcyujk";
    mostFrequentChar = mostFrequentCharacter (array, (int)strlen(array));
    assert(mostFrequentChar == 'b');
    unsigned int maxPossibleLength = -1;    //здесь будет максимальный unsigned int
    //если мы его поделим пополам, у нас получится как раз максимальный int
    //можно было через sizeof аналогично пытаться просчитать
    maxPossibleLength = maxPossibleLength/2;
    //
    //maxPossibleLength = __UP_MAX_DIRECT_SIZE;
    //maxPossibleLength = __UP_MAX_DIRECT_SIZE - 1;
    //Для подбора оптимального значения в __UP_MAX_DIRECT_SIZE нужно раскомментировать вышестоящую пару строк и сравнить время
    //Замеры всей секции тетсов показали, что
    //У меня на 16384 параллелизм все равно чуть-чуть медленнее
    //На 2147483640 разница около 10% производительности - лидирует многопоточных проход
    //Столь маленькая в процентном соотношении разница объясняется несколькими проходами по массиву в тестах: для подготовки данных и для проведения другого теста
    //Если замерять один вызов, прирост производительности заметен: 6.9 секунд вместо 13.5 - практически ровно в два раза возросла скорость выполнения этой длинной операции (хотя элементов в массиве на 1 больше)
    
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
    //без двух символов - уже должна преобладать 'b'
    mostFrequentChar = mostFrequentCharacter (array + sizeof(char) * 2, (int)length - 2);
    assert(mostFrequentChar == 'b');
    free(array);
    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("time spend: %f", time_spent);
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
        raise(__UP_SIGNAL_CODE);
        return 0; //0 я не собираюсь разыменовывать!
    }
    int *charStat = (int *) calloc((size_t) __UP_NUMBER_OF_ASCII_CHARS, (size_t) sizeof(int));
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = array[i];
        ++charStat[c];
    }
    return charStat;
}

char mostFrequentCharacter (char *array, int size) {
    if (!array || size <= 0) {
        //Ошибка: некорректные данные на входе!
        raise(__UP_SIGNAL_CODE);
        return 0;
    }
    if (size < 2) { //оптимизация очевидных случаев
        return array[0];    //если элементов в массиве 1 или 2, то первый элемент гарантированно попадает в ответ
    }
    char solution = 0;
    if (size < __UP_MAX_DIRECT_SIZE) {
        int *charStat = (int *) calloc((size_t) __UP_NUMBER_OF_ASCII_CHARS, (size_t) sizeof(int));
        for (size_t i = 0; i < size; ++i) {
            unsigned char c = array[i];
            if ((i < size - 1) && (charStat[c] >= size/2)) {
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
        pthread_t threads[__UP_NTHREADS];           //информация потоков
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
                raise(__UP_SIGNAL_CODE);
                return 0;
            }
        }
        for (size_t i = 0; i < __UP_NTHREADS; ++i) {
            int errcode = pthread_join(threads[i], (void *)&resultStatistics[i]);
            if (errcode) {
                fprintf(stderr,"pthread_join: %d\n",errcode);
                raise(__UP_SIGNAL_CODE);
                return 0;
            }
            if (!resultStatistics[i]) {
                fprintf(stderr, "pthread %zu finished badly\n", i);
                raise(__UP_SIGNAL_CODE);
                return 0;
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
