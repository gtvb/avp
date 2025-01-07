NAME := ave

CC := gcc
CFLAGS := -I/opt/homebrew/include -I./raylib/raylib-5.5/src -Wall -Wextra
LDFLAGS := -L/opt/homebrew/lib -L./raylib/raylib-5.5/src -lavcodec -lavformat -lavutil -lswscale -lswresample -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c $< -o $@

clean:
	rm -f $(OBJECTS) $(NAME)
