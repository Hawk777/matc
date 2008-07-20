#include "sockpath.h"
#include <stdlib.h>
#include <errno.h>



int sockpath_set_default(struct sockaddr_un *saddr) {
	const char *homedir;

	/* Get the home directory. */
	homedir = getenv("HOME");
	if (!homedir)
		homedir = "/";

	/* Check that homedir + "/.atcd-sock" will fit in the buffer. */
	if (strlen(homedir) + strlen("/.atcd-sock") + 1 > sizeof(saddr->sun_path)) {
		errno = ENAMETOOLONG;
		return -1;
	}

	/* Build the path. */
	strcpy(saddr->sun_path, homedir);
	strcat(saddr->sun_path, "/.actd-sock");
	return 0;
}

