#!/bin/bash
./front.out $1 -o out.ast
./back.out out.ast -o compiled.asm
./Processor/asm.out compiled.asm
./Processor/spu.out compiled.lol
