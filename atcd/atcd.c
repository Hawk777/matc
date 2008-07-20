#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include "auth.h"
#include "../shared/sockpath.h"



static const struct option longopts[] = {
	{"allow", required_argument, 0, 'a'},
	{"socket", required_argument, 0, 'S'},
	{0, 0, 0, 0}
};

static const char shortopts[] = "a:S:";

static pid_t cpid;



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



static int run_socket_once(const char *appname, int sockfd, int pipefd) {
	char databuf[1024], auxbuf[256];
	struct iovec iov;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	ssize_t ret;

	/* Receive a message. */
	iov.iov_base = databuf;
	iov.iov_len = sizeof(databuf);
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = auxbuf;
	msg.msg_controllen = sizeof(auxbuf);
	msg.msg_flags = 0;
	ret = recvmsg(sockfd, &msg, 0);
	if (ret < 0) {
		perror(appname);
		return -1;
	} else if (ret == 0) {
		return -1;
	}

	/* Scan the ancillary buffer to find the passed credential structure. */
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg && !(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS); cmsg = CMSG_NXTHDR(&msg, cmsg));
	if (!cmsg) {
		return -1;
	}

	/* Check for an acceptable UID. */
	if (auth_check_uid(((struct ucred *) CMSG_DATA(cmsg))->uid) < 0) {
		return -1;
	}

	/* Dump the received data into the pipe. */
	iov.iov_base = databuf;
	iov.iov_len = ret;
	while (iov.iov_len) {
		ret = writev(pipefd, &iov, 1);
		if (ret < 0) {
			perror(appname);
			return -1;
		}
		iov.iov_base += ret;
		iov.iov_len -= ret;
	}

	return 0;
}



static int run_parent(const char *appname, int listenfd, int pipefd) {
	int *talkfds = 0, *newtalkfds, maxfd, newfd, optval;
	size_t talkfds_count = 0, i;
	fd_set rfds, efds;

	for (;;) {
		/* Select on the talkfds plus the listenfd. */
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(listenfd, &rfds);
		FD_SET(listenfd, &efds);
		maxfd = listenfd;
		for (i = 0; i < talkfds_count; i++) {
			FD_SET(talkfds[i], &rfds);
			FD_SET(talkfds[i], &efds);
			if (talkfds[i] > maxfd)
				maxfd = talkfds[i];
		}
		if (select(maxfd + 1, &rfds, 0, &efds, 0) < 0) {
			perror(appname);
			return EXIT_FAILURE;
		}

		/* Walk through the talkfds, checking if each one is ready. */
		for (i = 0; i < talkfds_count; i++) {
			if (FD_ISSET(talkfds[i], &efds)) {
				/* Exception occurred. Close the socket. */
				close(talkfds[i]);
				memmove(talkfds + i, talkfds + i + 1, talkfds_count - i - 1);
				talkfds_count--;
				i--;
			} else if (FD_ISSET(talkfds[i], &rfds)) {
				/* Readable. Run the socket. */
				if (run_socket_once(appname, talkfds[i], pipefd) < 0) {
					close(talkfds[i]);
					memmove(talkfds + i, talkfds + i + 1, talkfds_count - i - 1);
					talkfds_count--;
					i--;
				}
			}
		}

		/* Check if we have any new connections to accept. */
		if (FD_ISSET(listenfd, &rfds)) {
			newfd = accept(listenfd, 0, 0);
			if (newfd < 0) {
				perror(appname);
				return EXIT_FAILURE;
			} else {
				optval = 1;
				if (setsockopt(newfd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) < 0) {
					perror(appname);
					return EXIT_FAILURE;
				}
				newtalkfds = realloc(talkfds, (talkfds_count + 1) * sizeof(*talkfds));
				if (!newtalkfds) {
					perror(appname);
					return EXIT_FAILURE;
				} else {
					talkfds = newtalkfds;
					talkfds[talkfds_count++] = newfd;
				}
			}
		}
	}
}



int main(int argc, char **argv) {
	int ret;
	unsigned long arg;
	char *endptr, **argarray;
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
			case 'a':
				arg = strtoul(optarg, &endptr, 10);
				if (*endptr) {
					if (auth_add_name(optarg) < 0) {
						perror(argv[0]);
						return EXIT_FAILURE;
					}
				} else {
					if (auth_add_uid(arg) < 0) {
						perror(argv[0]);
						return EXIT_FAILURE;
					}
				}
				break;

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
	if (bind(sockfd, (const struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
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

