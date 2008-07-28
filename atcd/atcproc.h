#if !defined ATCPROC_H
#define ATCPROC_H

/* Launches an ATC process. Returns 0 on success, -1 on failure. */
int atcproc_start(const char *game);

/* Stops any running process. Returns 0 on success, -1 on failure. */
int atcproc_stop(void);

/* Checks whether a child process is already running. Returns 1 if so, 0 if not. */
int atcproc_is_running(void);

/* Sends data to a running process. Returns 0 on success, -1 on failure. */
int atcproc_send(const char *string);

/* Specifies what function should be invoked when the ATC process dies. */
void atcproc_set_cb(void (*cb)(void));

/*
 * Race semantics are as follows:
 * Exactly one of the following will occur, sometime, for each successful
 * call to atcproc_start() (not both, and not neither, except in the case
 * when atcd is also dying):
 * - atcproc_stop() returns 0
 * - the callback set with atcproc_set_cb() is invoked
 *
 * Note however that the callback is invoked in signal-handler context and
 * thus must obey all the appropriate restrictions.
 */

#endif

