# host_slave_send锛欳IPMODE + CIPSEND 涓撻锛圵indows 鍙岀嚎绋嬶級
# 鐢ㄦ硶锛歮ake / make run / make clean

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=c99 -g
INC     := -I../../../src -I.
LIBS    :=

SRCS := ../../../src/lw_at_stream.c \
        ../../../src/lw_at_cmd_dict.c \
        ../../../src/lw_at_core.c \
        ../../../src/lw_at_transmit.c \
        link_q.c \
        send_cmd.c \
        slave_rt.c \
        host_api.c \
        scenes_gate.c \
        scenes_fixed.c \
        scenes_stream.c \
        scenes_edge.c \
        test_main.c

TARGET := test_host_slave_send

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS) $(LIBS)

run: all
	./$(TARGET)

clean:
	-del /Q $(TARGET) $(TARGET).exe 2>NUL || true

.PHONY: all run clean
