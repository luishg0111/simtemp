# File: Makefile
# Simple build for the user-space helper
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS ?=
TARGET ?= simtemp_ctl

all: $(TARGET)

$(TARGET): simtemp_ctl.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)
