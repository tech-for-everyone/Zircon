CC      := gcc
AR      := ar
ARFLAGS := rcs

BUILD   := build

CFLAGS  := -std=c99 -Wall -Wextra -pedantic -g
CPPFLAGS:= -I.gui -I.apps
LDFLAGS :=
LDLIBS  := -lm

GUI_SRC := $(wildcard .gui/*.c)
APP_SRC := $(wildcard .apps/*.c)
CORE_SRC:= $(wildcard *.c)

SRC     := $(CORE_SRC) $(GUI_SRC) $(APP_SRC)

OBJ     := $(patsubst %.c,$(BUILD)/%.o,$(SRC))
DEP     := $(OBJ:.o=.d)

TARGET  := zircond
LIBRARY := libzircon.a

.PHONY: all clean debug release

all: $(LIBRARY) $(TARGET)

debug: CFLAGS += -O0 -DDEBUG
debug: all

release: CFLAGS += -O2 -DNDEBUG
release: all

$(LIBRARY): $(OBJ)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD)
	rm -f $(TARGET) $(LIBRARY)

-include $(DEP)