#!/bin/bash
./front.out $1 -o out.ast
./back.out --spu out.ast -o compiled
./Processor/asm.out compiled.asm2
./Processor/spu.out compiled.lol
