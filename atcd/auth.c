#include "auth.h"
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>



static size_t allowed_count = 0, allowed_alloc = 0;
static uid_t *allowed = 0;



/* Ensures the array is large enough to store at least one more element. */
static int grow_array(void) {
	size_t new_alloc;
	uid_t *new;

	/* Check whether the array needs growing at all. */
	if (allowed_count < allowed_alloc)
		return 0;

	/* Decide how much to grow the array. */
	if (allowed_alloc == 0)
		new_alloc = 1;
	else if (allowed_alloc < 4)
		new_alloc = 4;
	else
		new_alloc = allowed_alloc * 2;

	/* Grow the array. */
	if (allowed)
		new = realloc(allowed, new_alloc * sizeof(*allowed));
	else
		new = malloc(new_alloc * sizeof(*allowed));
	if (!new)
		return -1;

	allowed = new;
	allowed_alloc = new_alloc;

	return 0;
}



int auth_init(void) {
	/* Initialize the authentication library to allow one UID by default: ourself. */
	auth_cleanup();
	return auth_add_uid(getuid());
}



void auth_cleanup(void) {
	/* Deallocate the array. */
	if (allowed) {
		free(allowed);
		allowed = 0;
		allowed_count = allowed_alloc = 0;
	}
}



int auth_add_uid(uid_t uid) {
	if (grow_array() < 0)
		return -1;
	allowed[allowed_count++] = uid;

	return 0;
}



int auth_add_name(const char *name) {
	struct passwd *pwd;

	/* Look up the name in /etc/passwd. */
	errno = 0;
	pwd = getpwnam(name);
	if (!pwd) {
		if (!errno)
			errno = ENOENT;
		return -1;
	}

	/* Add the corresponding UID. */
	return auth_add_uid(pwd->pw_uid);
}



int auth_check_uid(uid_t uid) {
	size_t i;

	/* Scan the array. */
	for (i = 0; i < allowed_count; ++i)
		if (allowed[i] == uid)
			return 0;

	errno = EACCES;
	return -1;
}

