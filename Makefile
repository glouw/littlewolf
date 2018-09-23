BIN = littlewolf

CFLAGS = -std=c99 -Wall -Wextra -pedantic -Ofast -flto -march=native

LDFLAGS = -lm -lSDL2

CC = gcc

SRC = main.c

all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) -o $(BIN)

run:
	./$(BIN)

clean:
	rm -f $(BIN)
