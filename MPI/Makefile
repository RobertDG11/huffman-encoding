.PHONY: clean

build:
	gcc -c -o aux_functions.o aux_functions.c
	mpicc -c -o main.o main.c
	mpicc -o test aux_functions.o main.o

clean:
	rm -f *.o
