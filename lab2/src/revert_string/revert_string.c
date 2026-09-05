#include "revert_string.h"
#include <string.h>

void RevertString(char *str) {
    if (str == NULL) return; // Защита от передачи пустого указателя

    int length = strlen(str);
    int left = 0;
    int right = length - 1;

    // Двигаемся с двух сторон к центру и меняем символы местами
    while (left < right) {
        char temp = str[left];
        str[left] = str[right];
        str[right] = temp;

        left++;
        right--;
    }
}

