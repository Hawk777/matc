#if !defined COMMANDS_H
#define COMMANDS_H

#include <stddef.h>

/* Attempts to parse the command string. On failure, returns false. On success, stores textual description into buffer (of size buflen), sets *terminal=true if the string is terminal or false if the string is syntactically valid but not finished, and returns true. */
bool parse_command(const char *cmd, char *buffer, size_t buflen, bool *terminal);

#endif

