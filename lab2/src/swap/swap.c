#include "swap.h"

void Swap(char *left, char *right)
{
	char temp = *left;  // Сохраняем значение, на которое указывает left
    *left = *right;     // Записываем значение из right в адрес left
    *right = temp;   
}
