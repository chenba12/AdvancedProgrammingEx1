CC=gcc
CFLAGS=-Wall -g
TARGET=myshell
SRC=myshell.c
OBJ=$(SRC:.c=.o)
HEADER=myshell.h

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ): $(SRC) $(HEADER)
	$(CC) $(CFLAGS) -c $(SRC)

clean:
	rm -f $(TARGET) $(OBJ)
