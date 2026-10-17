#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <getopt.h>
#include <stdint.h>

// Общие глобальные параметры
long long k = 0;
int pnum = 0;
long long mod = 0;
long long total_result = 1;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER; //инициализуруем мутекс

void *factorial_part(void *arg) {
    // Получаем номер потока от 0 до pnum - 1
    int thread_id = (int)(intptr_t)arg; //приводим аргумент(указатель) в целочисленный тип(intptr_t совпадает с размером указателя на ос)

    // Равномерно делим диапазон от 1 до k
    long long step = k / pnum;
    long long remainder = k % pnum; //чтобы не поетрять поток из целочисленного деления

    long long start = 1 + thread_id * step + (thread_id < remainder ? thread_id : remainder);
    long long end = start + step - 1 + (thread_id < remainder ? 1 : 0);

    long long local_prod = 1;
    for (long long i = start; i <= end; i++) {
        local_prod = (local_prod * (i % mod)) % mod;
    } //вычсиление факториала

    // Защита доступа к разделяемому результату с помощью мьютекса
    pthread_mutex_lock(&mut);
    total_result = (total_result * local_prod) % mod;
    pthread_mutex_unlock(&mut);

    return NULL;
}

int main(int argc, char **argv) {
    static struct option long_options[] = {
        {"k", required_argument, 0, 'k'},
        {"pnum", required_argument, 0, 'p'},
        {"mod", required_argument, 0, 'm'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "k:p:m:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'k':
                k = atoll(optarg);
                break;
            case 'p':
                pnum = atoi(optarg);
                break;
            case 'm':
                mod = atoll(optarg);
                break;
            default:
                fprintf(stderr, "Использование: %s -k <num> --pnum=<threads> --mod=<mod>\n", argv[0]);
                return 1;
        }
    }

    if (k < 0 || pnum <= 0 || mod <= 0) {
        fprintf(stderr, "Некорректные параметры: k >= 0, pnum > 0, mod > 0\n");
        return 1;
    }

    if (k == 0 || k == 1) {
        printf("%lld! mod %lld = %lld\n", k, mod, 1 % mod);
        return 0;
    }

    if (pnum > k) {
        pnum = (int)k;
    }

    pthread_t threads[pnum];

    // Запускаем потоки, передавая порядковый номер i вместо указателя на структуру
    for (int i = 0; i < pnum; i++) {
        pthread_create(&threads[i], NULL, factorial_part, (void *)(intptr_t)i ); //передаем указатель на число в функцию
    }

    // Дожидаемся завершения всех потоков
    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("%lld! mod %lld = %lld\n", k, mod, total_result);

    return 0;
}
