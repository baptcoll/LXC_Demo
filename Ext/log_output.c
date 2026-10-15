#define _POSIX_C_SOURCE 200809L

#include "log_output.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int log_output_init(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1)
        return -1;
    if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
        int saved_errno = errno;
        if (fd > STDERR_FILENO)
            close(fd);
        errno = saved_errno;
        return -1;
    }
    if (fd > STDERR_FILENO)
        close(fd);
    setvbuf(stdout, NULL, _IOLBF, 0);
    return 0;
}
