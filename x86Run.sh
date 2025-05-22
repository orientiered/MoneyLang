#!/bin/bash
./front.out $1 -o out.ast
./back.out out.ast -o $1
./$1.elf
