#if !defined COMMANDS_H
#define COMMANDS_H

/* Attempts to parse the command string. Returns the textual description on success, 0 on failure. On success, sets *terminal to 1 if the command is finished and can be submitted, 0 if it's valid so far but not finished. */
const char *parse_command(const char *cmd, int *terminal);

#endif

