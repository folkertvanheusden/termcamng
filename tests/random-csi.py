#! /usr/bin/python3

import random


while True:
    print('\033[', end='')

    n_par = random.randint(0, 10)
    for i in range(n_par):
        val = random.randint(-10, 300)
        print(f'{val}' if i == 0 else f';{val}', end='')

    ch = random.randint(0x40, 0x7e)
    print(f'{ch:c}', end='')
