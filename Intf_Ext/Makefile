CC = cc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
IPC_DIR = ../Ext

ifeq ($(shell uname -s),Linux)
LDLIBS += -lrt
endif

.PHONY: all clean test
all: intf_ext

intf_ext: main.c $(IPC_DIR)/oalIPC.c $(IPC_DIR)/oalIPC.h $(IPC_DIR)/socket_posix.c $(IPC_DIR)/socket_posix.h $(IPC_DIR)/log_output.c $(IPC_DIR)/log_output.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(IPC_DIR) main.c $(IPC_DIR)/oalIPC.c $(IPC_DIR)/socket_posix.c $(IPC_DIR)/log_output.c $(LDFLAGS) $(LDLIBS) -o $@

test: intf_ext
	python3 tests/test_intf_ext.py

clean:
	rm -f intf_ext
