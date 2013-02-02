OUT 		= tn

#variables
CC = gcc
CFLAGS = -Wall -g -Wextra -O2

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

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

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
		rm $(OBJ) $(OUT)
