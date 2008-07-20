#if !defined COMMANDS_H
#define COMMANDS_H

#include <stddef.h>

/* Attempts to parse the command string. On failure, returns -1. On success, stores textual description into buffer (of size buflen), sets *terminal=1 if the string is terminal or 0 if the string is syntactically valid but not finished, and returns 0. */
int parse_command(const char *cmd, char *buffer, size_t buflen, int *terminal);

#endif

