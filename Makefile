# Makefile for custom_FTL
# Builds every .c file in this folder into one program, and rebuilds
# anything that depends on a changed .h file automatically.
 
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -g
TARGET  := ftl
BUILD   := build
 
SRCS    := $(wildcard *.c)
OBJS    := $(SRCS:%.c=$(BUILD)/%.o)
DEPS    := $(OBJS:.o=.d)
 
.PHONY: all run clean
 
all: $(TARGET)
 
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
 
# -MMD -MP: record which headers each .c includes, so editing a .h triggers a rebuild
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
 
$(BUILD):
	mkdir -p $(BUILD)
 
# usage: make run ARGS="skewed 2"
run: $(TARGET)
	./$(TARGET) $(ARGS)
 
clean:
	rm -rf $(BUILD) $(TARGET)
 
-include $(DEPS)