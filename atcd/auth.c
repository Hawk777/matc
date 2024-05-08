#include "auth.h"
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>



static size_t allowed_count = 0, allowed_alloc = 0;
static uid_t *allowed = nullptr;



/* Ensures the array is large enough to store at least one more element. */
static bool grow_array(void) {
	/* Check whether the array needs growing at all. */
	if (allowed_count < allowed_alloc)
		return true;

	/* Decide how much to grow the array. */
	size_t new_alloc;
	if (allowed_alloc == 0)
		new_alloc = 1;
	else if (allowed_alloc < 4)
		new_alloc = 4;
	else
		new_alloc = allowed_alloc * 2;

	/* Grow the array. */
	uid_t *new;
	if (allowed)
		new = realloc(allowed, new_alloc * sizeof(*allowed));
	else
		new = malloc(new_alloc * sizeof(*allowed));
	if (!new)
		return false;

	allowed = new;
	allowed_alloc = new_alloc;

	return true;
}



/* Translates a string into a UID. Returns true on success, false on failure. */
static bool to_uid(const char *name, uid_t *uid) {
	/* Try first translating it numerically. */
	char *endptr;
	*uid = strtoul(name, &endptr, 10);
	if (*endptr == '\0')
		return true;

	/* Try translating it through /etc/passwd. */
	struct passwd *pwd;
	do {
		pwd = getpwnam(name);
	} while (!pwd && errno == EINTR);
	if (pwd) {
		*uid = pwd->pw_uid;
		return true;
	}

	return false;
}



bool auth_init(void) {
	/* Initialize the authentication library to allow one UID by default: ourself. */
	auth_cleanup();
	allowed = malloc(sizeof(*allowed));
	if (!allowed)
		return false;
	allowed_count = allowed_alloc = 1;
	allowed[0] = getuid();
	return true;
}



void auth_cleanup(void) {
	/* Deallocate the array. */
	if (allowed) {
		free(allowed);
		allowed = nullptr;
		allowed_count = allowed_alloc = 0;
	}
}



bool auth_add(const char *name) {
	/* Translate to UID. */
	uid_t uid;
	if (!to_uid(name, &uid))
		return false;

	/* Add to array. */
	if (!grow_array())
		return false;
	allowed[allowed_count++] = uid;
	return true;
}



bool auth_remove(const char *name) {
	/* Translate to UID. */
	uid_t uid;
	if (!to_uid(name, &uid))
		return false;

	/* Copy the array to itself, filtering out anything matching the target UID. */
	size_t wr = 0;
	for (size_t rd = 0; rd < allowed_count; rd++)
		if (allowed[rd] != uid)
			allowed[wr++] = allowed[rd];
	allowed_count = wr;

	return true;
}



bool auth_check(uid_t uid) {
	/* Scan the array. */
	for (size_t i = 0; i < allowed_count; ++i)
		if (allowed[i] == uid)
			return true;

	errno = EACCES;
	return false;
}



size_t auth_get_acl(const uid_t **acl) {
	*acl = allowed;
	return allowed_count;
}

