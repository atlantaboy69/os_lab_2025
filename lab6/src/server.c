#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

/*
 * Аргументы задачи, передаваемые в рабочий поток сервера:
 * begin - левая граница подотрезка вычислений (включительно)
 * end   - правая граница подотрезка вычислений (включительно)
 * mod   - модуль для вычислений
 */
struct FactorialArgs {
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
};

/*
 * Вычисление произведения всех чисел в замкнутом интервале [begin, end] по модулю mod.
 */
uint64_t Factorial(const struct FactorialArgs *args) {
    uint64_t ans = 1;

    // Если левая граница строго больше правой — диапазон пуст, возвращаем нейтральный элемент 1
    if (args->begin > args->end) {
        return 1;
    }

    for (uint64_t i = args->begin; i <= args->end; i++) {
        ans = MultModulo(ans, i, args->mod);
    }

    return ans;
}

/*
 * Точка входа для потока pthread:
 * Принимает указатель на FactorialArgs, возвращает результат через приведение
 * значения uint64_t к типу указателя (void *)(uintptr_t).
 */
void *ThreadFactorial(void *args) {
    struct FactorialArgs *fargs = (struct FactorialArgs *)args;
    uint64_t res = Factorial(fargs);
    return (void *)(uintptr_t)res;
}

int main(int argc, char **argv) {
    int tnum = -1;
    int port = -1;

    // Разбор именованных аргументов командной строки --port и --tnum
    while (true) {
        static struct option options[] = {
            {"port", required_argument, 0, 0},
            {"tnum", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) break;

        switch (c) {
        case 0:
            switch (option_index) {
            case 0:
                port = atoi(optarg);
                // Порты TCP лежат в диапазоне 1..65535
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "Error: Invalid port %d (must be 1-65535)\n", port);
                    return 1;
                }
                break;
            case 1:
                tnum = atoi(optarg);
                // Количество потоков должно быть положительным
                if (tnum <= 0) {
                    fprintf(stderr, "Error: tnum must be greater than 0\n");
                    return 1;
                }
                break;
            default:
                printf("Unknown option index: %d\n", option_index);
            }
            break;
        case '?':
            fprintf(stderr, "Error: Unknown argument encountered\n");
            return 1;
        default:
            fprintf(stderr, "getopt returned character code 0%o?\n", c);
            return 1;
        }
    }

    // Проверка наличия всех обязательных аргументов
    if (port == -1 || tnum == -1) {
        fprintf(stderr, "Usage: %s --port 20001 --tnum 4\n", argv[0]);
        return 1;
    }

    /*
     * 1. Создание TCP-сокета:
     * AF_INET     - стек протоколов IPv4
     * SOCK_STREAM - надежный двусторонний байтовый поток (TCP)
     */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error: socket creation failed");
        return 1;
    }

    /*
     * 2. Настройка сокета:
     * SO_REUSEADDR предотвращает ошибку "Address already in use",
     * позволяя повторно привязать порт, даже если предыдущее соединение
     * еще находится в системном состоянии TIME_WAIT.
     */
    int opt_val = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) {
        perror("Error: setsockopt(SO_REUSEADDR) failed");
        close(server_fd);
        return 1;
    }

    /*
     * 3. Формирование адреса привязки:
     * sin_family      - семейство адресов (AF_INET)
     * sin_port        - сетевой порядок байт (Big-Endian via htons)
     * INADDR_ANY      - прослушивание всех доступных сетевых интерфейсов
     */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
     * 4. Привязка сокета к локальному адресу и порту через bind()
     */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error: bind() failed");
        close(server_fd);
        return 1;
    }

    /*
     * 5. Перевод сокета в режим ожидания входящих соединений:
     * 128 - глубина очереди полностью установленных соединений (backlog).
     */
    if (listen(server_fd, 128) < 0) {
        perror("Error: listen() failed");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d with %d threads\n", port, tnum);

    /*
     * Основной цикл обслуживания клиентов:
     * Сервер последовательно принимает новые подключения от клиентов.
     */
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // accept() извлекает первое соединение из очереди и создает новый дескриптор client_fd
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Warning: accept() failed");
            continue;
        }

        /*
         * Цикл обработки запросов от подключенного клиента:
         * Одно соединение может присылать несколько вычислительных задач подряд.
         */
        while (true) {
            // Ожидаемый пакет данных: begin (8 байт) + end (8 байт) + mod (8 байт) = 24 байта
            unsigned int buffer_size = sizeof(uint64_t) * 3;
            char from_client[buffer_size];
            ssize_t bytes_read = 0;

            /*
             * TCP — потоковый протокол, не гарантирующий доставку всех байт за один вызов recv().
             * Цикл продолжается до тех пор, пока буфер не заполнится полностью.
             */
            while ((size_t)bytes_read < buffer_size) {
                ssize_t r = recv(client_fd, from_client + bytes_read, buffer_size - bytes_read, 0);
                if (r <= 0) {
                    bytes_read = r; // 0 означает EOF (клиент закрыл соединение), < 0 — сетевая ошибка
                    break;
                }
                bytes_read += r;
            }

            // Корректное закрытие сокета клиентом
            if (bytes_read == 0) break;
            if (bytes_read < 0) {
                perror("Warning: recv() error from client");
                break;
            }

            // Распаковываем полученные бинарные данные
            uint64_t begin = 0;
            uint64_t end = 0;
            uint64_t mod = 0;
            memcpy(&begin, from_client, sizeof(uint64_t));
            memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
            memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

            fprintf(stdout, "Task received: interval [%llu, %llu], mod %llu\n",
                    (unsigned long long)begin, (unsigned long long)end, (unsigned long long)mod);

            /*
             * Распараллеливание вычислений по tnum локальным потокам:
             * Балансировка: если отрезок не делится нацело, первые remainder потоков получают (step + 1) чисел.
             */
            pthread_t threads[tnum];
            struct FactorialArgs args[tnum];

            uint64_t total_elements = (begin <= end) ? (end - begin + 1) : 0;
            uint64_t step = total_elements / tnum;
            uint64_t remainder = total_elements % tnum;
            uint64_t current_begin = begin;

            for (int i = 0; i < tnum; i++) {
                uint64_t count = step + (i < (int)remainder ? 1 : 0);
                args[i].mod = mod;

                if (count > 0 && current_begin <= end) {
                    args[i].begin = current_begin;
                    args[i].end = current_begin + count - 1;
                    current_begin += count;
                } else {
                    // Пустой диапазон (потоков больше, чем чисел)
                    args[i].begin = 1;
                    args[i].end = 0;
                }

                if (pthread_create(&threads[i], NULL, ThreadFactorial, (void *)&args[i]) != 0) {
                    perror("Fatal: pthread_create failed on server");
                    close(client_fd);
                    close(server_fd);
                    return 1;
                }
            }

            // Ожидание завершения потоков и модульное свертывание результатов
            uint64_t total = 1;
            for (int i = 0; i < tnum; i++) {
                void *result = NULL;
                pthread_join(threads[i], &result);
                total = MultModulo(total, (uint64_t)(uintptr_t)result, mod);
            }

            printf("Local calculation result: %llu\n", (unsigned long long)total);

            // Отправка клиенту итогового ответа (8 байт)
            char buffer[sizeof(total)];
            memcpy(buffer, &total, sizeof(total));
            if (send(client_fd, buffer, sizeof(total), 0) < 0) {
                perror("Warning: send() response failed");
                break;
            }
        }

        // Завершение двустороннего обмена и освобождение дескриптора клиентского соединения
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}