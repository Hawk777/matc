#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <pwd.h>
#include <fcntl.h>
#include "auth.h"
#include "atcproc.h"
#include "../shared/sockpath.h"
#include "../shared/sockaddr_union.h"



static const struct option longopts[] = {
	{"socket", required_argument, 0, 'S'},
	{nullptr, 0, 0, 0}
};

static const char shortopts[] = "S:";

struct connection;
struct connection {
	struct connection *next;
	struct connection **prevptr;
	int fd;
	bool debug;
	uid_t user;
	char *username;
};

static struct connection *connections = nullptr;
static struct connection *pending = nullptr;

static volatile sig_atomic_t has_atc_exited = 0;



static void atc_death_cb(void) {
	has_atc_exited = 1;
}



static inline int set_socred(int fd, int enable) {
	int ret;
	do {
		ret = setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable));
	} while (ret < 0 && errno == EINTR);
	return ret;
}



static const struct connection CONN_ALL_IMPL, CONN_DEBUG_IMPL;
static const struct connection * const CONN_ALL = &CONN_ALL_IMPL;
static const struct connection * const CONN_DEBUG = &CONN_DEBUG_IMPL;
static inline void clputs(const struct connection *conn, const char *string) {

	if (conn != CONN_ALL && conn != CONN_DEBUG) {
		while (send(conn->fd, string, strlen(string), MSG_EOR) < 0 && errno == EINTR);
	} else {
		for (const struct connection *cur_conn = connections; cur_conn; cur_conn = cur_conn->next)
			if (conn == CONN_ALL || cur_conn->debug)
				clputs(cur_conn, string);
	}
}

static inline void clprintf(const struct connection *conn, const char *format, ...) {
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	clputs(conn, buffer);
}



static void server_command(const char *command, struct connection *conn) {
	if (strcmp(command, "help") == 0) {
		clputs(conn, "[server] supported commands on this server are:");
		clputs(conn, "[server] help");
		clputs(conn, "[server] debug");
		clputs(conn, "[server] nodebug");
		clputs(conn, "[server] allow <user>");
		clputs(conn, "[server] deny <user>");
		clputs(conn, "[server] acl");
		clputs(conn, "[server] users");
		clputs(conn, "[server] start [<map>]");
		clputs(conn, "[server] stop");
		clputs(conn, "[server] pause");
		clputs(conn, "[server] resume");
		clputs(conn, "[server] quit");
	} else if (strcmp(command, "debug") == 0) {
		conn->debug = true;
		clputs(conn, "[server] debug mode enabled");
	} else if (strcmp(command, "nodebug") == 0) {
		conn->debug = false;
		clputs(conn, "[server] debug mode disabled");
	} else if (memcmp(command, "allow ", 6) == 0) {
		if (!auth_add(command + 6)) {
			clputs(conn, "[server] error");
		} else {
			clputs(conn, "[server] OK");
		}
	} else if (memcmp(command, "deny ", 5) == 0) {
		if (!auth_remove(command + 5)) {
			clputs(conn, "[server] error");
		} else {
			clputs(conn, "[server] OK");
		}
	} else if (strcmp(command, "acl") == 0) {
		const uid_t *uids;
		size_t nuids = auth_get_acl(&uids);
		for (size_t i = 0; i < nuids; i++) {
			const struct passwd *pwd;
			do {
				pwd = getpwuid(uids[i]);
			} while (!pwd && errno == EINTR);
			if (pwd)
				clprintf(conn, "[server] %s", pwd->pw_name);
			else
				clprintf(conn, "[server] %u", uids[i]);
		}
	} else if (strcmp(command, "users") == 0) {
		for (const struct connection *cur_conn = connections; cur_conn; cur_conn = cur_conn->next)
			clprintf(conn, "[server] %s", cur_conn->username);
	} else if (memcmp(command, "start", 5) == 0 && (command[5] == '\0' || command[5] == ' ')) {
		if (atcproc_is_running()) {
			clputs(conn, "[server] game is already running");
		} else {
			if (atcproc_start(command[5] == '\0' ? nullptr : command + 6))
				clprintf(CONN_ALL, "[server] %s has started the game", conn->username);
		}
	} else if (strcmp(command, "stop") == 0) {
		if (atcproc_stop())
			clprintf(CONN_ALL, "[server] %s ended the game", conn->username);
	} else if (strcmp(command, "pause") == 0) {
		if (atcproc_pause())
			clprintf(CONN_ALL, "[server] %s paused the game", conn->username);
	} else if (strcmp(command, "resume") == 0) {
		if (atcproc_resume())
			clprintf(CONN_ALL, "[server] %s resumed the game", conn->username);
	} else if (strcmp(command, "quit") == 0) {
		for (const struct connection *cur_conn = connections; cur_conn; cur_conn = cur_conn->next)
			close(cur_conn->fd);
		atcproc_stop();
		printf("%s shut down the server\n", conn->username);
		exit(EXIT_SUCCESS);
	} else {
		clputs(conn, "[server] unknown command");
	}
}



static bool run_pending_connection_once(struct connection *conn) {
	/* Receive a message. */
	char databuf[256], auxbuf[256];
	struct iovec iov = {
		.iov_base = databuf,
		.iov_len = sizeof(databuf) - 1,
	};
	struct msghdr msg = {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = auxbuf,
		.msg_controllen = sizeof(auxbuf),
		.msg_flags = 0,
	};
	ssize_t ret;
	do {
		ret = recvmsg(conn->fd, &msg, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret <= 0)
		return false;
	databuf[ret] = '\0';

	/* Scan the ancillary buffer to find the passed credential structure. */
	struct cmsghdr *cmsg;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg && !(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS); cmsg = CMSG_NXTHDR(&msg, cmsg));
	if (!cmsg) {
		clputs(CONN_DEBUG, "[server] user denied for no credentials transmitted");
		clputs(conn, "MATC ACCESS");
		return false;
	}
	const struct ucred *cred = (const struct ucred *) CMSG_DATA(cmsg);

	/* Disable credential passing from now on. */
	if (set_socred(conn->fd, 0) < 0) {
		return false;
	}

	/* Look up the username. */
	struct passwd *pwd;
	do {
		pwd = getpwuid(cred->uid);
	} while (!pwd && errno == EINTR);
	if (!pwd) {
		clprintf(CONN_DEBUG, "[server] user denied for no passwd entry: %d", cred->uid);
		clputs(conn, "MATC ACCESS");
		return false;
	}

	/* Check for an acceptable UID. */
	if (!auth_check(cred->uid)) {
		clprintf(CONN_DEBUG, "[server] user denied by ACL: %s", pwd->pw_name);
		clputs(conn, "MATC ACCESS");
		return false;
	}

	/* Check for an acceptable protocol version. */
	if (strcmp(databuf, "MATC 1") != 0) {
		clprintf(CONN_DEBUG, "[server] user denied for bad protocol version: %s", pwd->pw_name);
		clputs(conn, "MATC VERSION");
		return false;
	}

	/* Store a copy of the UID and username. */
	conn->user = cred->uid;
	conn->username = strdup(pwd->pw_name);
	if (!conn->username) {
		clprintf(CONN_DEBUG, "[server] strdup failed saving username: %s", pwd->pw_name);
		return false;
	}

	/* Accept the new user! */
	clputs(conn, "MATC OK");

	/* Announce their arrival. */
	clprintf(CONN_ALL, "[server] %s has entered the game", conn->username);

	return true;
}



static bool run_connection_once(struct connection *conn) {
	/* Receive a message. */
	char databuf[256];
	ssize_t ret;
	do {
		ret = recv(conn->fd, databuf, sizeof(databuf) - 1, 0);
	} while (ret < 0 && errno == EINTR);
	if (ret <= 0)
		return false;
	databuf[ret] = '\0';

	/* Check if we received a chat message. */
	if (databuf[0] == '/') {
		/* Check if it's actually a server command. */
		if (databuf[1] == '/')
			server_command(databuf + 2, conn);
		else
			clprintf(CONN_ALL, "<%s> %s", conn->username, databuf + 1);
		return true;
	}

	/* Dump the received data to the atc process. */
	if (atcproc_is_running())
		atcproc_send(databuf);

	return true;
}



static int run_parent(const char *appname, int listenfd) {
	for (;;) {
		/* Load up all the socket FDs to select() on. */
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(listenfd, &rfds);
		int maxfd = listenfd;
		for (struct connection *conn = connections; conn; conn = conn->next) {
			FD_SET(conn->fd, &rfds);
			if (conn->fd > maxfd)
				maxfd = conn->fd;
		}
		for (struct connection *conn = pending; conn; conn = conn->next) {
			FD_SET(conn->fd, &rfds);
			if (conn->fd > maxfd)
				maxfd = conn->fd;
		}

		/* Careful! Manage the race between select()ing and getting SIGCHLD. */
		sigset_t mask, oldmask;
		sigfillset(&mask);
		sigprocmask(SIG_BLOCK, &mask, &oldmask);
		for (;;) {
			if (has_atc_exited) {
				clputs(CONN_ALL, "[server] the game has ended");
				has_atc_exited = 0;
			}
			if (pselect(maxfd + 1, &rfds, nullptr, nullptr, nullptr, &oldmask) >= 0)
				break;
			if (errno != EINTR) {
				perror(appname);
				return EXIT_FAILURE;
			}
		}
		sigprocmask(SIG_SETMASK, &oldmask, nullptr);

		/* First check for any progress in the connected FDs. */
		for (struct connection *conn = connections, *nextconn = conn ? conn->next : nullptr; conn; conn = nextconn, nextconn = conn ? conn->next : nullptr)
			if (FD_ISSET(conn->fd, &rfds))
				if (!run_connection_once(conn)) {
					/* Delete from linked list.*/
					if (conn->next)
						conn->next->prevptr = conn->prevptr;
					*(conn->prevptr) = conn->next;

					/* Shut down connection. */
					while (close(conn->fd) < 0 && errno == EINTR);
					clprintf(CONN_ALL, "[server] %s has exited the game", conn->username);
					free(conn->username);
					free(conn);
				}

		/* Next check for any progress in the pending FDs. */
		for (struct connection *conn = pending, *nextconn = conn ? conn->next : nullptr; conn; conn = nextconn, nextconn = conn ? conn->next : nullptr)
			if (FD_ISSET(conn->fd, &rfds)) {
				/* Delete from linked list. */
				if (conn->next)
					conn->next->prevptr = conn->prevptr;
				*(conn->prevptr) = conn->next;

				/* Handle the arrived packet. */
				if (!run_pending_connection_once(conn)) {
					/* Shut down the connection. */
					while (close(conn->fd) < 0 && errno == EINTR);
					free(conn);
				} else {
					/* Add to the connected list. */
					conn->next = connections;
					conn->prevptr = &connections;
					connections = conn;
					if (conn->next)
						conn->next->prevptr = &conn->next;
				}
			}

		/* Finally check if we have an incoming connection on the listener. */
		if (FD_ISSET(listenfd, &rfds)) {
			int newfd;
			do {
				newfd = accept(listenfd, nullptr, nullptr);
			} while (newfd < 0 && errno == EINTR);
			if (newfd >= 0) {
				/* Enable credential-passing. */
				if (set_socred(newfd, 1) < 0) {
					close(newfd);
				} else {
					/* Allocate a new connection structure. */
					struct connection *conn = malloc(sizeof(*conn));
					if (!conn) {
						close(newfd);
					} else {
						/* Initialize the new connection. */
						conn->fd = newfd;
						conn->debug = false;
						conn->user = 0;
						conn->username = nullptr;

						/* Add it to the pending list. */
						conn->next = pending;
						conn->prevptr = &pending;
						pending = conn;
						if (conn->next)
							conn->next->prevptr = &conn->next;
					}
				}
			}
		}
	}
}



int main(int argc, char **argv) {
	/* Initialize the authentication library. */
	if (!auth_init()) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Make the default socket path. */
	union sockaddr_union saddr;
	if (!sockpath_set_default(&saddr.sun)) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Scan command-line options. */
	int ret;
	while ((ret = getopt_long(argc, argv, shortopts, longopts, 0)) >= 0) {
		switch (ret) {
			case 'S':
				if (strlen(optarg) + 1 > sizeof(saddr.sun.sun_path)) {
					errno = ENAMETOOLONG;
					perror(argv[0]);
					return EXIT_FAILURE;
				}
				strcpy(saddr.sun.sun_path, optarg);
				break;

			default:
				fprintf(stderr, "%s: unrecognized argument\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	/* Create and initialize the socket. */
	int sockfd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	unlink(saddr.sun.sun_path);
	saddr.sun.sun_family = AF_UNIX;
	mode_t oldumask = umask(0);
	if (bind(sockfd, &saddr.s, sizeof(saddr)) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	umask(oldumask);
	if (listen(sockfd, 10) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Set the callback for child death. */
	atcproc_set_cb(&atc_death_cb);

	/* Run. */
	return run_parent(argv[0], sockfd);
}

