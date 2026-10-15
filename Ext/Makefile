CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

.PHONY: all clean
all: ext

ext: main.c socket_posix.c socket_posix.h
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c socket_posix.c $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f ext
