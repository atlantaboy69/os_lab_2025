#include "find_min_max.h"
#include <limits.h>
#include <unistd.h> // Обязательно добавляем для поддержки функции sleep

struct MinMax GetMinMax(int *array, unsigned int begin, unsigned int end) {
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  // ИСКУССТВЕННАЯ ЗАДЕРЖКА ДЛЯ ТЕСТА ТАЙМАУТА
  // Дочерний процесс замирает на 5 секунд, не успевая передать данные родителю
  sleep(5); 

  for (; begin < end; begin++) {
    if (array[begin] < min_max.min) {
      min_max.min = array[begin];
    }
    if (array[begin] > min_max.max) {
      min_max.max = array[begin];
    }
  }

  return min_max;
}
