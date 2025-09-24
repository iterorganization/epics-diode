#!/usr/bin/python3

from os import spawnlp
from re import split
import sys
import math
import fileinput

pv = {}

def check_pv(p):
    if p not in pv:
        return 0

    pv_values = pv[p]
    prev_val = '0'

    for v in pv_values:
        if v == '0' and prev_val == '0':
            pass
        elif int(prev_val) == int(v):
            pass
        else:
            if int(v) == (int(prev_val)+1):
                prev_val = v
            else:
#########                print("A {} {} {}".format(p, prev_val, v))
                return 0

    if int(prev_val) != 5:
#        print("B {} {}".format(p, prev_val))
        return 0

    return 1


for line in fileinput.input():
    split_line = line.split(' ')
    split_line[:] = [x for x in split_line if x]
    split_line = split_line[:-1]
    pv_name = split_line[0]
    pv_value = split_line[2]
    if pv_name not in pv:
        pv[pv_name] = []
    pv[pv_name].append(pv_value)


for x in range(0, 5):
    if not check_pv("poz:v{}".format(x+1)):
        sys.exit(1)


sys.exit(0)



