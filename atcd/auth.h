#if !defined AUTH_H
#define AUTH_H

#include <stddef.h>
#include <sys/types.h>

/* Initializes the authentication library. Returns 0 on success, -1 on failure. */
int auth_init(void);

/* Deinitializes the authentication library. */
void auth_cleanup(void);

/* Adds a user to the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_add(const char *name);

/* Removes a user from the list of allowed UIDs. Returns 0 on success, -1 on failure. */
int auth_remove(const char *name);

/* Checks whether a UID is permitted to connect. Returns 0 if allowed, -1 with errno=EACCES if not. */
int auth_check(uid_t uid);

/* Gets the list of permitted UIDs. Returns ACL size. */
size_t auth_get_acl(const uid_t **acl);

#endif

