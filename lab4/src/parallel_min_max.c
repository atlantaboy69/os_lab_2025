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
#include <signal.h> // Добавлено для работы с сигналами и SIGKILL

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

// Глобальные переменные, чтобы обработчик сигнала имел к ним доступ
pid_t *child_pids = NULL;
int total_pnum = 0;

// Обработчик сигнала SIGALRM, который сработает при истечении таймаута
void alarm_handler(int signum) {
    if (signum == SIGALRM) {
        printf("\n[Родитель] Время таймаута истекло! Отправка SIGKILL дочерним процессам...\n");
        for (int i = 0; i < total_pnum; i++) {
            if (child_pids[i] > 0) {
                kill(child_pids[i], SIGKILL);
            }
        }
    }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = -1; // Переменная для хранения значения таймаута
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    // Добавлена опция "timeout" на позицию индекса 3
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"timeout", required_argument, 0, 0}, // Новый флаг
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
          case 3: // Обработка таймаута
            timeout = atoi(optarg);
            if (timeout <= 0) {
              printf("Ошибка: timeout должен быть положительным числом\n");
              return 1;
            }
            break;
          case 4: 
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
    printf("Использование: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"num\"] [--by_files]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);

  // Выделяем память под массив PID дочерних процессов
  total_pnum = pnum;
  child_pids = malloc(sizeof(pid_t) * pnum);

  // Создаем pipes, если не передали флаг файлов
  int pipefd[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipefd[i]) < 0) {
        perror("Ошибка создания pipe");
        free(array);
        free(child_pids);
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
        free(child_pids); // Освобождаем память в дочернем процессе
        return 0;
      } else {
        // Код родительского процесса: сохраняем PID запущенного потомка
        child_pids[i] = child_pid;
        active_child_processes += 1;
      }

    } else {
      printf("Ошибка вызова fork!\n");
      free(array);
      free(child_pids);
      return 1;
    }
  }

  // Установка таймера, если параметр timeout был передан
  if (timeout > 0) {
    signal(SIGALRM, alarm_handler); // Регистрируем наш обработчик сигнала
    alarm(timeout);                 
  }

  // Ожидание завершения всех дочерних процессов
  while (active_child_processes > 0) {
    int status;
    // -1 означает ждать любого дочернего процесса
    pid_t pid = waitpid(-1, &status, WNOHANG); 

    if (pid > 0) {
        // Процесс успешно завершился или был убит
        active_child_processes -= 1;
        if (WIFSIGNALED(status)) {
            printf("[Родитель] Дочерний процесс %d прерван сигналом %d\n", pid, WTERMSIG(status));
        }
    } else if (pid == 0) {
        // Ни один процесс еще не завершился. Родитель не завис («No Hang»)!
        // Здесь можно сделать микро-паузу, чтобы не загружать CPU на 100%
        usleep(10000); 
    } else {
        // Ошибка (например, если waitpid прерван обработчиком сигнала SIGALRM)
        break; 
    }
}

  // Если процессы были убиты по таймауту, прекращаем выполнение, так как данные не собраны полностью
  if (active_child_processes > 0) {
      printf("[Родитель] Работа программы прервана по таймауту.\n");
      free(array);
      free(child_pids);
      return 1;
  }

  // Отменяем alarm, если процессы успели завершиться раньше времени
  if (timeout > 0) {
      alarm(0); 
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
  free(child_pids);

  printf("Минимум: %d\n", min_max.min);
  printf("Максимум: %d\n", min_max.max);
  printf("Затраченное время: %fмс\n", elapsed_time);
  fflush(NULL);
  return 0;
}
