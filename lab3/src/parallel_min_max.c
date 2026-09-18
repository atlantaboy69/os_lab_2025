#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("Ошибка: seed должен быть положительным числом\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("Ошибка: array_size должен быть положительным числом\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("Ошибка: pnum должен быть положительным числом\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Индекс %d вне диапазона опций\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt вернул код символа 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Обнаружены лишние аргументы\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Использование: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);

  // Создаем pipes, если не передали флаг файлов
  int pipefd[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipefd[i]) < 0) {
        perror("Ошибка создания pipe");
        free(array);
        return 1;
      }
    }
  }

  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  // Простой расчет шага для каждого процесса
  int step = array_size / pnum;

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      if (child_pid == 0) {
        // Дочерний процесс: определяем начало и конец диапазона
        int begin = i * step;
        int end = (i == pnum - 1) ? array_size : (i + 1) * step;

        struct MinMax local_min_max = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[32];
          sprintf(filename, "min_max_%d.txt", i);
          FILE *fp = fopen(filename, "w");
          fprintf(fp, "%d %d\n", local_min_max.min, local_min_max.max);
          fclose(fp);
        } else {
          close(pipefd[i][0]); // Закрываем чтение
          write(pipefd[i][1], &local_min_max, sizeof(struct MinMax));
          close(pipefd[i][1]);
        }
        free(array);
        return 0;
      }

    } else {
      printf("Ошибка вызова fork!\n");
      free(array);
      return 1;
    }
  }

  // Ожидание завершения всех дочерних процессов
  while (active_child_processes > 0) {
    wait(NULL);
    active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  // Сбор результатов от дочерних процессов
  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      char filename[32];
      sprintf(filename, "min_max_%d.txt", i);
      FILE *fp = fopen(filename, "r");
      if (fp != NULL) {
        fscanf(fp, "%d %d", &min, &max);
        fclose(fp);
        remove(filename);
      }
    } else {
      struct MinMax local_res;
      close(pipefd[i][1]); // Закрываем запись
      read(pipefd[i][0], &local_res, sizeof(struct MinMax));
      close(pipefd[i][0]);
      min = local_res.min;
      max = local_res.max;
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Минимум: %d\n", min_max.min);
  printf("Максимум: %d\n", min_max.max);
  printf("Затраченное время: %fмс\n", elapsed_time);
  fflush(NULL);
  return 0;
}