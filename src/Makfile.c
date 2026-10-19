CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
GTKFLAGS = `pkg-config --cflags --libs gtk+-3.0`
SRCS = src/main.c src/pcb.c src/scheduler.c src/memory.c src/banker.c src/ipc.c src/logger.c
OBJS = $(SRCS:.c=.o)
TARGET = mini_os

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(GTKFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) logs.txt

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
