OUT 		= tn

#variables
CC = gcc
CFLAGS = -Wall -g -Wextra -O2

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(OUT)

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OUT): $(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
		rm $(OBJ) $(OUT)
