#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

/*
 * Структура контекста задачи для отдельного потока клиента:
 * Каждый поток работает ровно с одним удаленным сервером.
 */
struct ClientThreadTask {
    struct Server server; // Адрес и порт целевого сервера
    uint64_t begin;       // Начало диапазона для этого сервера
    uint64_t end;         // Конец диапазона для этого сервера
    uint64_t mod;         // Модуль вычисления
    uint64_t result;      // Промежуточный ответ, полученный от сервера
    bool success;         // Флаг успешного завершения сетевого обмена
};

/*
 * Функция потока-клиента:
 * Изолированно подключается к назначенному серверу, отсылает диапазон,
 * вычитывает результат и записывает его в структуру задачи.
 */
void *ProcessServer(void *arg) {
    struct ClientThreadTask *task = (struct ClientThreadTask *)arg;
    task->success = false;
    task->result = 1;

    // Если отрезок пустой, ничего не запрашиваем у сервера (нейтральный результат = 1)
    if (task->begin > task->end) {
        task->result = 1;
        task->success = true;
        return NULL;
    }

    /*
     * 1. Разрешение сетевого адреса узла (DNS или строковый IPv4)
     */
    struct hostent *hostname = gethostbyname(task->server.ip);
    if (hostname == NULL) {
        fprintf(stderr, "Error: gethostbyname() failed for host %s\n", task->server.ip);
        return NULL;
    }

    /*
     * 2. Заполнение структуры sockaddr_in для подключения
     */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)task->server.port);
    memcpy(&server_addr.sin_addr.s_addr, hostname->h_addr_list[0], hostname->h_length);
    /*
     * 3. Создание клиентского TCP-сокета
     */
    int sck = socket(AF_INET, SOCK_STREAM, 0);
    if (sck < 0) {
        perror("Error: socket() failed in client thread");
        return NULL;
    }

    /*
     * 4. Установка соединения с удаленным сервером
     */
    if (connect(sck, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "Error: connect() failed to %s:%d\n", task->server.ip, task->server.port);
        close(sck);
        return NULL;
    }

    /*
     * 5. Упаковка и отправка данных: [begin: 8 байт][end: 8 байт][mod: 8 байт]
     */
    char payload[sizeof(uint64_t) * 3];
    memcpy(payload, &task->begin, sizeof(uint64_t));
    memcpy(payload + sizeof(uint64_t), &task->end, sizeof(uint64_t));
    memcpy(payload + 2 * sizeof(uint64_t), &task->mod, sizeof(uint64_t));

    if (send(sck, payload, sizeof(payload), 0) < 0) {
        perror("Error: send() failed in client thread");
        close(sck);
        return NULL;
    }

    /*
     * 6. Вычитывание ответа сервера (ровно 8 байт) с обработкой фрагментации TCP
     */
    char response[sizeof(uint64_t)];
    ssize_t bytes_read = 0;
    while ((size_t)bytes_read < sizeof(uint64_t)) {
        ssize_t r = recv(sck, response + bytes_read, sizeof(uint64_t) - bytes_read, 0);
        if (r <= 0) break;
        bytes_read += r;
    }

    if (bytes_read != sizeof(uint64_t)) {
        fprintf(stderr, "Error: Corrupted response received from %s:%d\n", task->server.ip, task->server.port);
        close(sck);
        return NULL;
    }

    // Сохраняем полученный результат
    memcpy(&task->result, response, sizeof(uint64_t));
    task->success = true;

    // Закрываем сетевой сокет после завершения обмена
    close(sck);
    return NULL;
}

/*
 * Чтение файла конфигурации серверов:
 * Считывает файл построчно, разбирает записи вида "<ip>:<port>".
 * Динамически расширяет массив серверов по мере необходимости.
 */
int ReadServersFile(const char *path, struct Server **servers_out) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("Error: Failed to open servers file");
        return -1;
    }

    int capacity = 8;
    int count = 0;
    struct Server *list = malloc(sizeof(struct Server) * capacity);
    if (!list) {
        perror("Error: malloc failed");
        fclose(f);
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Удаление символов перевода строки
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        if (strlen(line) == 0) continue; // Пропуск пустых строк

        char ip_buf[MAX_IP_LEN];
        int port = 0;

        // Формат парсинга: строка до ':' сохраняется в ip_buf, число после ':' — в port
        if (sscanf(line, "%254[^:]:%d", ip_buf, &port) == 2) {
            if (count >= capacity) {
                capacity *= 2;
                struct Server *new_list = realloc(list, sizeof(struct Server) * capacity);
                if (!new_list) {
                    perror("Error: realloc failed");
                    free(list);
                    fclose(f);
                    return -1;
                }
                list = new_list;
            }
            strncpy(list[count].ip, ip_buf, MAX_IP_LEN - 1);
            list[count].ip[MAX_IP_LEN - 1] = '\0';
            list[count].port = port;
            count++;
        }
    }

    fclose(f);
    *servers_out = list;
    return count;
}

int main(int argc, char **argv) {
    uint64_t k = (uint64_t)-1;
    uint64_t mod = (uint64_t)-1;
    /*
     * Буфер размером 255 байт соответствует POSIX-лимиту NAME_MAX
     * для одного компонента имени файла в файловой системе Linux.
     */
    char servers_path[255] = {'\0'};

    // Разбор аргументов командной строки --k, --mod, --servers
    while (true) {
        static struct option options[] = {
            {"k", required_argument, 0, 0},
            {"mod", required_argument, 0, 0},
            {"servers", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) break;

        switch (c) {
        case 0:
            switch (option_index) {
            case 0:
                if (!ConvertStringToUI64(optarg, &k)) {
                    fprintf(stderr, "Error: Invalid argument --k '%s'\n", optarg);
                    return 1;
                }
                break;
            case 1:
                if (!ConvertStringToUI64(optarg, &mod)) {
                    fprintf(stderr, "Error: Invalid argument --mod '%s'\n", optarg);
                    return 1;
                }
                break;
            case 2:
                strncpy(servers_path, optarg, sizeof(servers_path) - 1);
                servers_path[sizeof(servers_path) - 1] = '\0';
                break;
            default:
                printf("Unknown option index: %d\n", option_index);
            }
            break;
        case '?':
            fprintf(stderr, "Error: Invalid CLI arguments\n");
            return 1;
        default:
            fprintf(stderr, "getopt returned character code 0%o?\n", c);
            return 1;
        }
    }

    if (k == (uint64_t)-1 || mod == (uint64_t)-1 || strlen(servers_path) == 0) {
        fprintf(stderr, "Usage: %s --k 1000 --mod 5 --servers /path/to/file\n", argv[0]);
        return 1;
    }

    // Загружаем список доступных серверов из файла
    struct Server *servers = NULL;
    int servers_num = ReadServersFile(servers_path, &servers);
    if (servers_num <= 0) {
        fprintf(stderr, "Error: No reachable servers found in file '%s'\n", servers_path);
        free(servers);
        return 1;
    }

    pthread_t threads[servers_num];
    struct ClientThreadTask tasks[servers_num];

    /*
     * Распределение отрезка [1, k] по серверам:
     * Если k не делится нацело на servers_num, остаток распределяется по +1
     * среди первых серверов.
     */
    uint64_t step = k / servers_num;
    uint64_t remainder = k % servers_num;
    uint64_t current_begin = 1;

    for (int i = 0; i < servers_num; i++) {
        uint64_t count = step + (i < (int)remainder ? 1 : 0);
        tasks[i].server = servers[i];
        tasks[i].mod = mod;

        if (count > 0 && current_begin <= k) {
            tasks[i].begin = current_begin;
            tasks[i].end = current_begin + count - 1;
            current_begin += count;
        } else {
            tasks[i].begin = 1;
            tasks[i].end = 0;
        }

        /*
         * Запуск отдельного потока клиента на каждый сервер.
         * Соединения и сетевой ввод/вывод выполняются параллельно.
         */
        if (pthread_create(&threads[i], NULL, ProcessServer, &tasks[i]) != 0) {
            perror("Error: pthread_create failed on client");
            free(servers);
            return 1;
        }
    }

    // Синхронизация потоков и аггрегация ответов серверов
    uint64_t total = 1;
    bool all_success = true;

    for (int i = 0; i < servers_num; i++) {
        pthread_join(threads[i], NULL);
        if (!tasks[i].success) {
            fprintf(stderr, "Calculation error occurred on server %s:%d\n",
                    tasks[i].server.ip, tasks[i].server.port);
            all_success = false;
        } else {
            total = MultModulo(total, tasks[i].result, mod);
        }
    }

    // Вывод итогового факториала k! % mod
    if (all_success) {
        printf("answer: %llu\n", (unsigned long long)total);
    } else {
        fprintf(stderr, "Failure: Failed to calculate answer due to server errors.\n");
    }

    free(servers);
    return all_success ? 0 : 1;
}