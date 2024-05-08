#if !defined AUTH_H
#define AUTH_H

#include <stddef.h>
#include <sys/types.h>

/* Initializes the authentication library. Returns true on success, false on failure. */
bool auth_init(void);

/* Deinitializes the authentication library. */
void auth_cleanup(void);

/* Adds a user to the list of allowed UIDs. Returns true on success, false on failure. */
bool auth_add(const char *name);

/* Removes a user from the list of allowed UIDs. Returns true on success, false on failure. */
bool auth_remove(const char *name);

/* Checks whether a UID is permitted to connect. Returns true if allowed, false with errno=EACCES if not. */
bool auth_check(uid_t uid);

/* Gets the list of permitted UIDs. Returns ACL size. */
size_t auth_get_acl(const uid_t **acl);

#endif

