#if !defined AUTH_H
#define AUTH_H

#include <sys/types.h>

/* Initializes the authentication library. Returns 0 on success, -1 on failure. */
int auth_init(void);

/* Deinitializes the authentication library. */
void auth_cleanup(void);

/* Adds a UID to the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_add_uid(uid_t uid);

/* Translates a username to a UID and adds it to the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_add_name(const char *name);

/* Removes a UID from the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_remove_uid(uid_t uid);

/* Translates a username to a UID and removes it from the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_remove_name(const char *name);

/* Checks whether a UID is permitted to connect. Returns 0 if allowed, -1 with errno=EACCES if not. */
int auth_check_uid(uid_t uid);

#endif

