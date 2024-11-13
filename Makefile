all:
	clang-3.5 -std=c99 -Wall -pedantic -Werror *.c –L. -lruntime -o main

run: all
	LD_PRELOAD="${PWD}/libruntime.so" ./main -p 3 10 20 30

clean:
	rm -f main.c
	rm -f *.log