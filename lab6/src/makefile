# Компилятор и флаги сборки
CC = gcc
# -Wall -Wextra: вывод всех предупреждений
# -pthread: подключение библиотеки POSIX Threads для сервера и клиента
# -O2: оптимизация второго уровня
CFLAGS = -Wall -Wextra -pthread -O2

# Утилита для создания статических архивов (библиотек)
AR = ar
# r: заменить/добавить файлы в архив; c: создать архив без вывода сообщения; s: создать индекс символов
ARFLAGS = rcs

# Цель по умолчанию: собираем оба исполняемых файла
all: server client

# 1. Сборка статической библиотеки libcommon.a из объектного файла common.o
libcommon.a: common.o
	$(AR) $(ARFLAGS) $@ $^

# 2. Компиляция общего модуля common.c в объектный файл
common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c -o $@

# 3. Линковка сервера со статической библиотекой libcommon.a
server: server.o libcommon.a
	$(CC) $(CFLAGS) server.o -L. -lcommon -o $@

# Компиляция server.c в объектный файл
server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c -o $@

# 4. Линковка клиента со статической библиотекой libcommon.a
client: client.o libcommon.a
	$(CC) $(CFLAGS) client.o -L. -lcommon -o $@

# Компиляция client.c в объектный файл
client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c -o $@

# Очистка всех объектных файлов, статических библиотек и бинарников
clean:
	rm -f *.o *.a server client

# Объявление псевдоцелей во избежание конфликта с одноименными файлами
.PHONY: all clean