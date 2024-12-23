all:
	clang-3.5  -std=c99 -Wall -pedantic -Werror *.c -o main -L. -lruntime

run: all 
	LD_PRELOAD="${PWD}/libruntime.so" ./main -p 3 4 5 6

clean:
	rm -f main.c
	rm -f *.log