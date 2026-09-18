#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  char *seed = (argc > 1) ? argv[1] : "42";
  char *array_size = (argc > 2) ? argv[2] : "100";

  pid_t pid = fork();

  if (pid < 0) {
    perror("Ошибка вызова fork");
    return 1;
  }

  if (pid == 0) {
    // NULL в конце списка обязателен
    char *args[] = {"./sequential_min_max", seed, array_size, NULL};

    execv("./sequential_min_max", args);

    // Сюда управление попадает только если запуск сорвался (например, файл не найден)
    perror("Ошибка вызова execv");
    return 1;
  }

  // Родительский процесс просто ждет окончания работы sequential_min_max
  wait(NULL);

  return 0;
}