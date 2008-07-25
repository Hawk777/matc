#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>
#include "auth.h"
#include "list.h"
#include "../shared/sockpath.h"



static const struct option longopts[] = {
	{"socket", required_argument, 0, 'S'},
	{0, 0, 0, 0}
};

static const char shortopts[] = "S:";

static pid_t cpid;

struct connection;
struct connection {
	int fd, debug;
	uid_t user;
	char *username;
	struct list_node link;
};

static struct list_node connections = LIST_EMPTY_NODE(connections);
static struct list_node pending = LIST_EMPTY_NODE(pending);



static void sig_handler(int signum) {
	int status;

	if (signum == SIGINT) {
		/* We need to kill the child as well. */
		kill(cpid, SIGINT);
		do {
			waitpid(cpid, &status, 0);
		} while (WIFSTOPPED(status));
		_exit(EXIT_SUCCESS);
	} else if (signum == SIGCHLD) {
		/* Die when the child dies. */
		_exit(EXIT_SUCCESS);
	}
}



static inline int set_socred(int fd, int enable) {
	return setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable));
}



static inline void send_one(const char *msg, const struct connection *conn) {
	send(conn->fd, msg, strlen(msg), MSG_EOR);
}

static inline void send_some(const char *msg, int only_debug) {
	struct list_node *cur, *prev;

	list_for_each(connections, cur, prev)
		if (!only_debug || list_entry(cur, struct connection, link)->debug)
			send_one(msg, list_entry(cur, struct connection, link));
}

static inline void send_debug(const char *msg) {
	send_some(msg, 1);
}

static inline void send_all(const char *msg) {
	send_some(msg, 0);
}

static void send_chat(const char *msg, const struct connection *sender) {
	char buffer[256];

	/* Stage the message. */
	snprintf(buffer, sizeof(buffer), "<%s> %s", sender->username, msg);

	/* Send the message to all connected clients. */
	send_all(buffer);
}



static void server_command(const char *command, struct connection *conn) {
	if (strcmp(command, "help") == 0) {
		send_one("[atcd] supported commands on this server are:", conn);
		send_one("[atcd] help", conn);
		send_one("[atcd] debug", conn);
		send_one("[atcd] nodebug", conn);
		send_one("[atcd] allow", conn);
		send_one("[atcd] deny", conn);
		send_one("[atcd] acl", conn);
		send_one("[atcd] users", conn);
	} else if (strcmp(command, "debug") == 0) {
		conn->debug = 1;
		send_one("[atcd] debug mode enabled", conn);
	} else if (strcmp(command, "nodebug") == 0) {
		conn->debug = 0;
		send_one("[atcd] debug mode disabled", conn);
	} else if (memcmp(command, "allow ", strlen("allow ")) == 0) {
		if (auth_add(command + strlen("allow ")) < 0) {
			send_one("[atcd] error", conn);
		} else {
			send_one("[atcd] OK", conn);
		}
	} else if (memcmp(command, "deny ", strlen("deny ")) == 0) {
		if (auth_remove(command + strlen("deny ")) < 0) {
			send_one("[atcd] error", conn);
		} else {
			send_one("[atcd] OK", conn);
		}
	} else if (strcmp(command, "acl") == 0) {
		uid_t *uids;
		size_t nuids, i;
		struct passwd *pwd;
		char buffer[256];
		nuids = auth_get_acl(&uids);
		for (i = 0; i < nuids; i++) {
			pwd = getpwuid(uids[i]);
			if (pwd)
				snprintf(buffer, sizeof(buffer), "[atcd] %s", pwd->pw_name);
			else
				snprintf(buffer, sizeof(buffer), "[atcd] %u", uids[i]);
			send_one(buffer, conn);
		}
	} else if (strcmp(command, "users") == 0) {
		struct list_node *cur, *prev;
		char buffer[256];
		list_for_each(connections, cur, prev) {
			snprintf(buffer, sizeof(buffer), "[atcd] %s", list_entry(cur, struct connection, link)->username);
			send_one(buffer, conn);
		}
	} else {
		send_one("[atcd] unknown command", conn);
	}
}



static int run_pending_connection_once(const char *appname, struct connection *conn) {
	char databuf[256], auxbuf[256];
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	ssize_t ret;
	struct passwd *pwd;
	const struct ucred *cred;

	/* Receive a message. */
	iov.iov_base = databuf;
	iov.iov_len = sizeof(databuf) - 1;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = auxbuf;
	msg.msg_controllen = sizeof(auxbuf);
	msg.msg_flags = 0;
	ret = recvmsg(conn->fd, &msg, 0);
	if (ret <= 0)
		return -1;
	databuf[ret] = '\0';

	/* Scan the ancillary buffer to find the passed credential structure. */
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg && !(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS); cmsg = CMSG_NXTHDR(&msg, cmsg));
	if (!cmsg) {
		send_debug("[atcd] user denied for no credentials transmitted");
		send_one("MATC ACCESS", conn);
		return -1;
	}
	cred = (const struct ucred *) CMSG_DATA(cmsg);

	/* Disable credential passing from now on. */
	if (set_socred(conn->fd, 0) < 0) {
		return -1;
	}

	/* Look up the username. */
	pwd = getpwuid(cred->uid);
	if (!pwd) {
		snprintf(databuf, sizeof(databuf), "[atcd] user denied for no passwd entry: %d", cred->uid);
		send_debug(databuf);
		send_one("MATC ACCESS", conn);
		return -1;
	}

	/* Check for an acceptable UID. */
	if (auth_check(cred->uid) < 0) {
		snprintf(databuf, sizeof(databuf), "[atcd] user denied by ACL: %s", pwd->pw_name);
		send_debug(databuf);
		send_one("MATC ACCESS", conn);
		return -1;
	}

	/* Check for an acceptable protocol version. */
	if (strcmp(databuf, "MATC 1") != 0) {
		snprintf(databuf, sizeof(databuf), "[atcd] user denied for bad protocol version: %s", pwd->pw_name);
		send_debug(databuf);
		send_one("MATC VERSION", conn);
		return -1;
	}

	/* Store a copy of the UID and username. */
	conn->user = cred->uid;
	conn->username = strdup(pwd->pw_name);
	if (!conn->username) {
		snprintf(databuf, sizeof(databuf), "[atcd] malloc failed saving username: %s", pwd->pw_name);
		send_debug(databuf);
		return -1;
	}

	/* Accept the new user! */
	send_one("MATC OK", conn);
	return 0;
}



static int run_connection_once(struct connection *conn, int pipefd) {
	char databuf[256];
	ssize_t ret;
	char *ptr;
	size_t left;

	/* Receive a message. */
	ret = recv(conn->fd, databuf, sizeof(databuf) - 1, 0);
	if (ret <= 0)
		return -1;
	databuf[ret] = '\0';

	/* Check if we received a chat message. */
	if (databuf[0] == '/') {
		/* Check if it's actually a server command. */
		if (databuf[1] == '/')
			server_command(databuf + 2, conn);
		else
			send_chat(databuf + 1, conn);
		return 0;
	}

	/* Dump the received data into the pipe. */
	ptr = databuf;
	left = ret;
	while (left) {
		ret = write(pipefd, ptr, left);
		if (ret < 0)
			return -1;
		ptr += ret;
		left -= ret;
	}

	return 0;
}



static int run_parent(const char *appname, int listenfd, int pipefd) {
	int maxfd, newfd;
	fd_set rfds;
	struct list_node *prev, *cur;
	struct connection *conn;

	for (;;) {
		/* Select on everything. */
		FD_ZERO(&rfds);
		FD_SET(listenfd, &rfds);
		maxfd = listenfd;
		list_for_each(connections, cur, prev) {
			conn = list_entry(cur, struct connection, link);
			FD_SET(conn->fd, &rfds);
			if (conn->fd > maxfd)
				maxfd = conn->fd;
		}
		list_for_each(pending, cur, prev) {
			conn = list_entry(cur, struct connection, link);
			FD_SET(conn->fd, &rfds);
			if (conn->fd > maxfd)
				maxfd = conn->fd;
		}
		if (select(maxfd + 1, &rfds, 0, 0, 0) < 0) {
			perror(appname);
			return EXIT_FAILURE;
		}

		/* First check for any progress in the connected FDs. */
		list_for_each(connections, cur, prev) {
			conn = list_entry(cur, struct connection, link);
			if (FD_ISSET(conn->fd, &rfds)) {
				if (run_connection_once(conn, pipefd) < 0) {
					list_del(cur, prev);
					close(conn->fd);
					free(conn->username);
					free(conn);
				}
			}
		}

		/* Next check for any progress in the pending FDs. */
		list_for_each(pending, cur, prev) {
			conn = list_entry(cur, struct connection, link);
			if (FD_ISSET(conn->fd, &rfds)) {
				list_del(cur, prev);
				if (run_pending_connection_once(appname, conn) < 0) {
					close(conn->fd);
					free(conn);
				} else {
					list_add(cur, &connections);
				}
			}
		}

		/* Finally check if we have an incoming connection on the listener. */
		if (FD_ISSET(listenfd, &rfds)) {
			newfd = accept(listenfd, 0, 0);
			if (newfd >= 0) {
				/* Enable credential-passing. */
				if (set_socred(newfd, 1) < 0) {
					close(newfd);
				} else {
					/* Allocate a new connection structure. */
					conn = malloc(sizeof(*conn));
					if (!conn) {
						close(newfd);
					} else {
						/* Initialize the new connection and link it into the pending list. */
						conn->fd = newfd;
						conn->debug = 0;
						conn->user = 0;
						conn->username = 0;
						list_add(&conn->link, &pending);
					}
				}
			}
		}
	}
}



int main(int argc, char **argv) {
	int ret;
	mode_t oldumask;
	char **argarray;
	int pipefds[2];
	int piperead, pipewrite, sockfd;
	struct sockaddr_un saddr;
	struct sigaction sigact;

	/* Initialize the authentication library. */
	if (auth_init() < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Make the default socket path. */
	if (sockpath_set_default(&saddr) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Scan command-line options. */
	while ((ret = getopt_long(argc, argv, shortopts, longopts, 0)) >= 0) {
		switch (ret) {
			case 'S':
				if (strlen(optarg) + 1 > sizeof(saddr.sun_path)) {
					errno = ENAMETOOLONG;
					perror(argv[0]);
					return EXIT_FAILURE;
				}
				strcpy(saddr.sun_path, optarg);
				break;

			default:
				fprintf(stderr, "%s: unrecognized argument\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	/* Create a pipe to run between the parent atcd process and the child atc process. */
	if (pipe(pipefds) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	piperead = pipefds[0];
	pipewrite = pipefds[1];

	/* Build an array to hold the remaining command-line parameters to send to atc. */
	argarray = malloc(sizeof(*argarray) * (argc - optind + 2));
	if (!argarray) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	argarray[0] = "atc";
	memcpy(argarray + 1, argv + optind, (argc - optind) * sizeof(char *));
	argarray[argc - optind + 1] = 0;

	/* Set up the handler for SIGINT and SIGCHLD. */
	sigact.sa_handler = &sig_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	if (sigaction(SIGINT, &sigact, 0) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	sigact.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sigact, 0) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Create and initialize the socket. */
	sockfd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	unlink(saddr.sun_path);
	saddr.sun_family = AF_UNIX;
	oldumask = umask(0);
	if (bind(sockfd, (const struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	umask(oldumask);
	if (listen(sockfd, 10) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Fork! */
	cpid = fork();
	if (cpid < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	} else if (cpid == 0) {
		/* Child. */
		/* Close the socket. */
		close(sockfd);
		/* Close the write side of the pipe. */
		close(pipewrite);
		/* Copy the read side of the pipe to become stdin. */
		if (dup2(piperead, 0) < 0) {
			perror(argv[0]);
			return EXIT_FAILURE;
		}
		/* Close the old read handle. */
		close(piperead);
		/* Run atc! */
		execvp("atc", argarray);
		perror(argv[0]);
		return EXIT_FAILURE;
	} else {
		/* Parent. */
		/* Close the read side of the pipe. */
		close(piperead);
		/* Run magic! */
		return run_parent(argv[0], sockfd, pipewrite);
	}

	return 0;
}

