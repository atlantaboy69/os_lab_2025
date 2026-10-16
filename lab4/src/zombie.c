#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

int main() {
    pid_t child_pid = fork();

    if (child_pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (child_pid == 0) {
        // Код потомка: завершается сразу
        printf("[Потомок] Завершена работа (PID: %d). Теперь зомби...\n", getpid());
        exit(0);
    } else {
        // Код родителя: спит и игнорирует wait()
        printf("[Родитель] Потомок запущен с PID: %d\n", child_pid);
        printf("[Родитель] Задержка 20 секунд. Проверьте статус процесса в другом терминале!\n");
        printf("Команда для проверки: ps -aux | grep defunct\n");
        
        sleep(20); 

        printf("[Родитель] Задержка прошла, завершение работы, зомби-процесса нет.\n");
    }

    return 0;
}
