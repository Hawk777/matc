#if !defined SOCKPATH_H
#define SOCKPATH_H

#include <sys/un.h>
bool sockpath_set_default(struct sockaddr_un *saddr);

#endif

