CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -pedantic -O2 -g
AR      = ar
ARFLAGS = rcs

GUI_SRC = $(wildcard .gui/*.c)
GUI_OBJ = $(GUI_SRC:.c=.o)

APP_SRC = $(wildcard .apps/*.c)
APP_OBJ = $(APP_SRC:.c=.o)

SRC_C   = $(wildcard *.c)
OBJ     = $(SRC_C:.c=.o) $(GUI_OBJ) $(APP_OBJ)

.PHONY: all clean lib test

all: libzircon.a zircond

# Static library for linking into CodeOS kernel or userspace
libzircon.a: $(GUI_OBJ) $(APP_OBJ)
	$(AR) $(ARFLAGS) $@ $^

# Zircon daemon — standalone userspace process (testable on Linux)
zircond: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

.gui/%.o: .gui/%.c .gui/%.h
	$(CC) $(CFLAGS) -I.gui -c $< -o $@

.apps/%.o: .apps/%.c .apps/%.h
	$(CC) $(CFLAGS) -I.apps -I.gui -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I.gui -I.apps -c $< -o $@

clean:
	rm -f $(OBJ) libzircon.a zircond
