
# If CC is not already defined, set it to gcc
CC ?= gcc

# make it possible to cross-compile
# if CROSS_COMPILE is empty, set CC to gcc, otherwise use the CROSS_COMPILE
ifeq ($(CROSS_COMPILE),)		
	CC := gcc
else
	CC := $(CROSS_COMPILE)gcc
endif

#add flags
CFLAGS = -Wall -Wextra -g
#list of source files
SRC = aesdsocket.c
#respective list of object files
OBJ = $(SRC:.c=.o)
#target name
TARGET = aesdsocket

all: $(TARGET)

# rule to build target
$(TARGET) : $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)
	
# rule to get the OBJ, where $< is the first dependency .c and $@ is the target
%.o : %.c 
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f $(TARGET) $(OBJ)
	
# mark all and clean as commands
.PHONY: all clean
