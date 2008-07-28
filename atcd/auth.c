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



/* Translates a string into a UID. Returns 0 on success, -1 on failure. */
static int to_uid(const char *name, uid_t *uid) {
	char *endptr;
	struct passwd *pwd;

	/* Try first translating it numerically. */
	*uid = strtoul(name, &endptr, 10);
	if (*endptr == '\0')
		return 0;

	/* Try translating it through /etc/passwd. */
	do {
		pwd = getpwnam(name);
	} while (!pwd && errno == EINTR);
	if (pwd) {
		*uid = pwd->pw_uid;
		return 0;
	}

	return -1;
}



int auth_init(void) {
	/* Initialize the authentication library to allow one UID by default: ourself. */
	auth_cleanup();
	allowed = malloc(sizeof(*allowed));
	if (!allowed)
		return -1;
	allowed_count = allowed_alloc = 1;
	allowed[0] = getuid();
	return 0;
}



void auth_cleanup(void) {
	/* Deallocate the array. */
	if (allowed) {
		free(allowed);
		allowed = 0;
		allowed_count = allowed_alloc = 0;
	}
}



int auth_add(const char *name) {
	uid_t uid;

	/* Translate to UID. */
	if (to_uid(name, &uid) < 0)
		return -1;

	/* Add to array. */
	if (grow_array() < 0)
		return -1;
	allowed[allowed_count++] = uid;
	return 0;
}



int auth_remove(const char *name) {
	uid_t uid;
	size_t rd, wr;

	/* Translate to UID. */
	if (to_uid(name, &uid) < 0)
		return -1;

	/* Copy the array to itself, filtering out anything matching the target UID. */
	for (rd = wr = 0; rd < allowed_count; rd++)
		if (allowed[rd] != uid)
			allowed[wr++] = allowed[rd];
	allowed_count = wr;

	return 0;
}



int auth_check(uid_t uid) {
	size_t i;

	/* Scan the array. */
	for (i = 0; i < allowed_count; ++i)
		if (allowed[i] == uid)
			return 0;

	errno = EACCES;
	return -1;
}



size_t auth_get_acl(const uid_t **acl) {
	*acl = allowed;
	return allowed_count;
}

