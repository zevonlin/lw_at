# LW-AT 最小可运行例程（Windows 事件驱动定时器 + MCU 风格主循环）
# 用法：make / make run / make clean

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=c99 -g
INC     := -I../../src -I../../port -Icmd

SRCS := ../../src/lw_at_stream.c \
        ../../src/lw_at_cmd_dict.c \
        ../../src/lw_at_core.c \
        ../../src/lw_at_transmit.c \
        ../../port/lw_at_port_win.c \
        cmd/lw_at_cmd_ex.c \
        main.c

TARGET := lw_at_example

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS)

run: all
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(TARGET).exe

.PHONY: all run clean
