CC=gcc
CFLAGS=-I.


all : parallel_min_max process_memory psum

# --- Новые правила для parallel_sum ---

# Сборка объектного файла библиотеки
sum_lib.o : sum_lib.c sum_lib.h
	$(CC) -c sum_lib.c $(CFLAGS)

# Упаковка в статическую библиотеку libsum.a
libsum.a : sum_lib.o
	ar rcs libsum.a sum_lib.o

# Компиляция исполняемого файла с линковкой библиотеки и флага -pthread
psum : parallel_sum.c utils.o libsum.a
	$(CC) -o psum parallel_sum.c utils.o -L. -lsum -pthread $(CFLAGS)


# --- Старые правила ---

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

process_memory : process_memory.c
	$(CC) -o process_memory process_memory.c $(CFLAGS)

utils.o : utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

find_min_max.o : utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

clean :
	rm -f utils.o find_min_max.o sum_lib.o libsum.a parallel_min_max process_memory psum
