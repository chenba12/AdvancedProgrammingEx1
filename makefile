CC=gcc
CFLAGS=-Wall -g
TARGET=myshell
SRC=myshell.c
OBJ=$(SRC:.c=.o)
ZIP_NAME=315800961_987654321.zip

.PHONY: all clean zip

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean:
	rm -f $(TARGET) $(OBJ) $(ZIP_NAME)

zip: clean all
	zip $(ZIP_NAME) $(SRC) Makefile README.txt