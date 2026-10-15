#ifndef LOG_OUTPUT_H
#define LOG_OUTPUT_H

/* Append stdout and stderr to path. Call before writing to either stream.
 * Returns 0 on success, or -1 with errno set. */
int log_output_init(const char *path);

#endif
