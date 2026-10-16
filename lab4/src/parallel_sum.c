#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>

#include "utils.h"
#include "sum_lib.h"

void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  return (void *)(size_t)Sum(sum_args);
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  // Парсинг аргументов командной строки
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
        {"threads_num", required_argument, 0, 't'},
        {"seed", required_argument, 0, 's'},
        {"array_size", required_argument, 0, 'a'},
        {0, 0, 0, 0}
    };

    int c = getopt_long(argc, argv, "t:s:a:", long_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 't':
        threads_num = atoi(optarg);
        break;
      case 's':
        seed = atoi(optarg);
        break;
      case 'a':
        array_size = atoi(optarg);
        break;
      default:
        printf("Использование: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n", argv[0]);
        return 1;
    }
  }

  if (threads_num <= 0 || array_size <= 0 || seed <= 0) {
    printf("Ошибка: укажите все параметры корректно!\n");
    printf("Пример: %s --threads_num 4 --seed 42 --array_size 1000000\n", argv[0]);
    return 1;
  }

  // Генерация массива (вне замера времени)
  int *array = malloc(sizeof(int) * array_size);
  if (!array) {
    perror("malloc error");
    return 1;
  }
  GenerateArray(array, array_size, seed);

  // Выделение памяти под дескрипторы потоков и структуры аргументов
  pthread_t *threads = malloc(sizeof(pthread_t) * threads_num);
  struct SumArgs *args = malloc(sizeof(struct SumArgs) * threads_num);

  // Старт замера времени вычислений
  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Разбиение массива на равные части между потоками
  int chunk_size = array_size / threads_num;
  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * chunk_size;
    args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * chunk_size;

    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  // Сборка результатов от всех потоков
  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    void *partial_sum;
    pthread_join(threads[i], &partial_sum);
    total_sum += (int)(size_t)partial_sum;
  }

  // Остановка замера времени
  struct timeval end_time;
  gettimeofday(&end_time, NULL);

  double elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;

  // Очистка выделенной памяти
  free(array);
  free(threads);
  free(args);

  printf("Total: %d\n", total_sum);
  printf("Time elapsed: %.4f ms\n", elapsed_time);

  return 0;
}
