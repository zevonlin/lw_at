# lw_at_core ??????
# ???make / make run / make clean

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=c99 -g
INC     := -I../../../src -I../../fixtures

SRCS := ../../../src/lw_at_stream.c \
        ../../../src/lw_at_cmd_dict.c \
        ../../../src/lw_at_core.c \
        ../../../src/lw_at_transmit.c \
        ../../fixtures/test_port.c \
        ../../fixtures/test_cmd.c \
        test_main.c

TARGET := test_lw_at_core

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS)

run: all
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(TARGET).exe

.PHONY: all run clean
