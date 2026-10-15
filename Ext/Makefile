CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

ifeq ($(shell uname -s),Linux)
LDLIBS += -lrt
endif

.PHONY: all clean test
all: ext

ext: main.c oalIPC.c oalIPC.h socket_posix.c socket_posix.h log_output.c log_output.h
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c oalIPC.c socket_posix.c log_output.c $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f ext test_oalIPC

test_oalIPC: tests/test_oalIPC.c oalIPC.c oalIPC.h socket_posix.c socket_posix.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -I. tests/test_oalIPC.c oalIPC.c socket_posix.c $(LDFLAGS) $(LDLIBS) -o $@

test: ext test_oalIPC
	./test_oalIPC
	python3 tests/test_ext.py
