#!/bin/bash

CFLAGS="-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -Iraylib-5.5_linux_amd64/include"
LFLAGS="-Lraylib-5.5_linux_amd64/lib/ -l:libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11"

if [ $# -eq 0 ] || [ $1 == debug ]; then
    gcc -g -DDEBUG $CFLAGS poker_linux.c -o poker_linux $LFLAGS
elif [ $1 == release ]; then
    gcc -O2 -DNDEBUG $CFLAGS poker_linux.c -o poker_linux $LFLAGS
else
	echo Unkown argument!
fi
