OUT 		= tn

#variables
CC = gcc
CFLAGS = -Wall -g -Wextra -O2

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

.PHONY: clean doc tags all

all: $(OUT)

.c.o:
	$(CC) -c $(CFLAGS) $<

main.o: main.c sort.h task.h
	$(CC) -c $(CFLAGS) main.c -o main.o

sort.o: sort.c sort.h task.h
	$(CC) -c $(CFLAGS) sort.c -o sort.o

task.o: task.c config.h json.h task.h
	$(CC) -c $(CFLAGS) task.c -o task.o

json.o: json.c json.h
	$(CC) -c $(CFLAGS) json.c -o json.o

tasklist.o: tasklist.c tasklist.h task.h cursutil.h
	$(CC) -c $(CFLAGS) tasklist.c -o tasklist.o

cursutil.o: cursutil.c cursutil.h
	$(CC) -c $(CFLAGS) cursutil.c -o cursutil.o

$(OUT): $(OBJ)
	$(CC) -lncursesw -o $@ $(OBJ)

clean:
	rm $(OBJ) $(OUT)

doc:
	doxygen Doxyfile

tags:
	ctags *.c *.h
