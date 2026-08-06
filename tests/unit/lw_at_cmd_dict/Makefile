# lw_at_cmd_dict 妯″潡鍗曞厓娴嬭瘯
# 鐢ㄦ硶锛歮ake / make run / make clean

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Werror -std=c99 -g
INC     := -I../../../src

SRCS := ../../../src/lw_at_cmd_dict.c test_main.c
TARGET := test_lw_at_cmd_dict

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(INC) -o $@ $(SRCS)

run: all
	./$(TARGET)

clean:
	-rm -f $(TARGET) $(TARGET).exe

.PHONY: all run clean
