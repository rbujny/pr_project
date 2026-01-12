SOURCES=$(wildcard *.c)
HEADERS=$(SOURCES:.c=.h)
FLAGS=-g

all: main

debug: FLAGS += -DDEBUG
debug: main

main: $(SOURCES) $(HEADERS) Makefile
	mpicc $(SOURCES) $(FLAGS) -o main

clear: clean

clean:
	rm -f main a.out

run: main
	mpirun -oversubscribe -np 6 ./main
