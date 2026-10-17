#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mut_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mut_B = PTHREAD_MUTEX_INITIALIZER;

void *thread_one(void *arg) {
    printf("[Поток 1]: Попытка захватить mut_A...\n");
    pthread_mutex_lock(&mut_A);
    printf("[Поток 1]: Захватил mut_A. Имитация работы...\n");

    sleep(1); // Гарантирует, что Поток 2 успеет захватить mut_B

    printf("[Поток 1]: Попытка захватить mut_B...\n");
    pthread_mutex_lock(&mut_B); // Блокировка навсегда, первый поток не отпустит mut_a пока не захватит mut_b
    printf("[Поток 1]: Захватил mut_B (никогда не выполнится).\n");

    pthread_mutex_unlock(&mut_B);
    pthread_mutex_unlock(&mut_A);
    return NULL;
}

void *thread_two(void *arg) {
    printf("[Поток 2]: Попытка захватить mut_B...\n");
    pthread_mutex_lock(&mut_B);
    printf("[Поток 2]: Захватил mut_B. Имитация работы...\n");

    sleep(1); // Гарантирует, что Поток 1 успеет захватить mut_A

    printf("[Поток 2]: Попытка захватить mut_A...\n");
    pthread_mutex_lock(&mut_A); // Блокировка навсегда
    printf("[Поток 2]: Захватил mut_A (никогда не выполнится).\n");

    pthread_mutex_unlock(&mut_A);
    pthread_mutex_unlock(&mut_B);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, thread_one, NULL);
    pthread_create(&t2, NULL, thread_two, NULL);

    pthread_join(t1, NULL); //ждет завершения первого потока
    pthread_join(t2, NULL);

    printf("Завершение программы.\n");
    return 0;
}
