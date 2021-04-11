#!/usr/bin/env python3
import random
import string
import sys

if len(sys.argv) < 2: 
    print("Must provide # of lines as argument")
    exit()

for i in range(int(sys.argv[1])):
    x = ''
    for i in range(40):
        x += random.choice(string.ascii_letters + ' ')
    print(x)

