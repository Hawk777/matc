#if !defined SOCKADDR_UNION_H
#define SOCKADDR_UNION_H

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

union sockaddr_union {
	struct sockaddr s;
	struct sockaddr_un sun;
};

#endif

