

CC ?= gcc

ifeq ($(CROSS_COMPILE),)
	CC := gcc
else
	CC := $(CROSS_COMPILE)gcc
endif


SRC = writer.c
OBJ = $(SRC:.c=.o)
TARGET = writer


all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

%.o: %.c 
	$(CC) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

.PHONY: all clean