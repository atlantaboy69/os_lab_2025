#!/bin/bash

if [ $# -eq 0 ]; then
    echo "Количество: 0"
    echo "Среднее арифметическое: 0"
    exit 0
fi

count=$#
sum=0

for num in "$@"; do
    sum=$((sum + num))
done

# Считаем среднее с помощью встроенного awk (дробная часть вернется автоматически)
average=$(awk "BEGIN {print $sum / $count}")

echo "Количество чисел: $count"
echo "Среднее арифметическое: $average"
