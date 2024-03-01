# If typing just 'make', convert to 'make all'
.DEFAULT_GOAL := all

# Compiler and linker flags
CC        := gcc
INC_DIRS  := -Isrc
LIB_DIRS  := 
C_FLAGS   := -Wall -Wextra
LD_FLAGS  := 
MAKEFLAGS += -j8

# Dependencies and object files
_DEPS     := common.h
DEPS      := $(patsubst %,src/%,$(_DEPS))
_OBJ      := common.o
OBJ       := $(patsubst %,bin/obj/%,$(_OBJ))

# Create object files
bin/obj/%.o: src/%.c $(DEPS) | bin/obj
	$(CC) $(C_FLAGS) $(INC_DIRS) -c -o $@ $<

# Link object files to create final executable
bin/server.out: $(OBJ) bin/obj/server.o Makefile
	$(CC) $(LIB_DIRS) $(OBJ) bin/obj/server.o -o bin/server.out $(LD_FLAGS)

bin/client.out: $(OBJ) bin/obj/client.o Makefile
	$(CC) $(LIB_DIRS) $(OBJ) bin/obj/client.o -o bin/client.out $(LD_FLAGS)

# Create directories when needed
bin/obj: | bin
	mkdir bin/obj 

bin:
	mkdir bin

# When typing 'make', compile and link executables
all: bin/server.out bin/client.out

# When typing 'make clean', clean up object files and executables
.PHONY: clean
clean:
	rm -f bin/obj/*.o bin/*.out
