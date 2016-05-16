.PHONY: all clean

TARGET = nic_monitor

CC = gcc
RM = rm -f
CFLAGS = -g -Wall
LDFLAGS =

SOURCES = $(shell find -name "*.c")
OBJS = $(SOURCES:.c=.o)

all: clean $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning..."
	@$(RM) $(TARGET) $(OBJS)
