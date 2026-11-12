CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Werror
SDL_FLAGS = $(shell sdl2-config --cflags --libs)

all: chip8

chip8: chip8.c
	$(CC) chip8.c -o chip8 $(CFLAGS) $(SDL_FLAGS)

debug: CFLAGS += -DDEBUG -g
debug: clean chip8

clean:
	rm -f chip8

.PHONY: all clean debug
