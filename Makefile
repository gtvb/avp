NAME := ave

CC := gcc
CFLAGS := -I/opt/homebrew/include -I/raylib/raylib-5.5 -Wall -Wextra
LDFLAGS := -L/opt/homebrew/lib -lavcodec -lavformat -lavutil -lswscale -lraylib

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c $< -o $@

clean:
	rm -f $(OBJECTS) $(NAME)
