# ????? Windows ????????
# ???make / make run / make clean

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=c99 -g
INC     := -I../../../src -I.
LIBS    :=

SRCS := ../../../src/lw_at_stream.c \
        ../../../src/lw_at_cmd_dict.c \
        ../../../src/lw_at_core.c \
        ../../../src/lw_at_transmit.c \
        link_q.c \
        link_impair.c \
        hs_cmd.c \
        slave_rt.c \
        host_api.c \
        scenes_normal.c \
        scenes_frag.c \
        scenes_noise.c \
        scenes_buffer.c \
        scenes_stress.c \
        scenes_transmit.c \
        scenes_upstream.c \
        test_main.c

TARGET := test_host_slave_win

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS) $(LIBS)

run: all
	./$(TARGET)

clean:
	-del /Q $(TARGET) $(TARGET).exe 2>NUL || true

.PHONY: all run clean
