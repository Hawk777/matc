#include "sockpath.h"
#include <stdlib.h>
#include <errno.h>



bool sockpath_set_default(struct sockaddr_un *saddr) {
	/* Get the home directory. */
	const char *homedir = getenv("HOME");
	if (!homedir)
		homedir = "/";

	/* Check that homedir + "/.atcd-sock" will fit in the buffer. */
	if (strlen(homedir) + strlen("/.atcd-sock") + 1 > sizeof(saddr->sun_path)) {
		errno = ENAMETOOLONG;
		return false;
	}

	/* Build the path. */
	strcpy(saddr->sun_path, homedir);
	strcat(saddr->sun_path, "/.atcd-sock");
	return true;
}

