#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Алгоритм вычисления (a * b) % mod:
 * Прямое произведение uint64_t a * b может превысить 2^64 - 1 и переполниться
 * еще до применения операции взятия остатка.
 * Метод "удвоения" заменяет умножение на логарифмическое число модульных сложений.
 */
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
    uint64_t result = 0;
    a = a % mod; // Предварительная редукция первого сомножителя

    while (b > 0) {
        // Если младший бит числа b равен 1, прибавляем текущее 'a' к результату
        if (b % 2 == 1) {
            result = (result + a) % mod;
        }
        // Удваиваем основание: (a * 2) % mod
        a = (a * 2) % mod;
        // Сдвигаем b вправо на 1 бит (деление нацело на 2)
        b /= 2;
    }

    return result % mod;
}

/*
 * Преобразование строкового представления аргумента в uint64_t:
 * - Сбрасывает errno перед вызовом strtoull.
 * - Проверяет флаг ERANGE на предмет выхода за границы uint64_t.
 * - Проверяет указатель end: строка должна быть непустой и прочитанной до конца ('\0').
 */
bool ConvertStringToUI64(const char *str, uint64_t *val) {
    char *end = NULL;
    errno = 0;
    unsigned long long i = strtoull(str, &end, 10);

    // Проверка на арифметическое переполнение диапазона
    if (errno == ERANGE) {
        fprintf(stderr, "Error: Value out of uint64_t range: %s\n", str);
        return false;
    }

    // Проверка: строка была пустой, произошла ошибка или остались непрочитанные символы
    if (errno != 0 || end == str || *end != '\0') {
        return false;
    }

    *val = (uint64_t)i;
    return true;
}