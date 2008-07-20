#include <curses.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "commands.h"
#include "../shared/sockpath.h"



static void safe_endwin(void) {
	int saved_errno = errno;
	endwin();
	errno = saved_errno;
}



static ssize_t dosend(int sockfd, const char *buffer, size_t length) {
	struct msghdr msg;
	struct iovec iov;
	char cmsgbuf[CMSG_SPACE(sizeof(struct ucred))];

	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	msg.msg_flags = 0;

	iov.iov_base = (char *) buffer;
	iov.iov_len = length;

	CMSG_FIRSTHDR(&msg)->cmsg_level = SOL_SOCKET;
	CMSG_FIRSTHDR(&msg)->cmsg_type = SCM_CREDENTIALS;
	CMSG_FIRSTHDR(&msg)->cmsg_len = CMSG_LEN(sizeof(struct ucred));
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->pid = getpid();
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->uid = getuid();
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->gid = getgid();

	return sendmsg(sockfd, &msg, MSG_NOSIGNAL);
}



static int run(const char *appname, int sockfd) {
	char current_input[1024] = "", new_input[1024], output[1024];
	int ch, terminal;

	for (;;) {
		/* Get a character. */
		ch = getch();
		if (ch == ERR) {
			safe_endwin();
			perror(appname);
			return EXIT_FAILURE;
		}

		/* See what it is. */
		if (ch == 4) {
			/* Control-D -> terminate */
			endwin();
			return EXIT_SUCCESS;
		} else if (ch == 12) {
			/* Control-L -> refresh screen -> send immediately */
			new_input[0] = 12;
			if (dosend(sockfd, new_input, 1) < 0) {
				safe_endwin();
				perror(appname);
				return EXIT_FAILURE;
			}
		} else if (ch == ' ') {
			/* Space -> could be used at the termination of the game -> send immediately */
			new_input[0] = ' ';
			if (dosend(sockfd, new_input, 1) < 0) {
				safe_endwin();
				perror(appname);
				return EXIT_FAILURE;
			}
		} else if (ch == '\r' || ch == KEY_ENTER) {
			/* Enter -> send only if our current input is terminal */
			if (parse_command(current_input, output, sizeof(output), &terminal) == 0 && terminal) {
				strcat(current_input, "\r\n");
				if (dosend(sockfd, current_input, strlen(current_input)) < 0) {
					safe_endwin();
					perror(appname);
					return EXIT_FAILURE;
				}
				current_input[0] = '\0';
				printw("\r");
				clrtoeol();
				refresh();
			}
		} else if (ch == KEY_BACKSPACE || ch == 8 || ch == 127) {
			/* Backspace -> if current input nonempty then remove last char */
			if (current_input[0] != '\0') {
				current_input[strlen(current_input) - 1] = '\0';
				parse_command(current_input, output, sizeof(output), &terminal);
				printw("\r%s", output);
				clrtoeol();
				refresh();
			}
		} else {
			/* Otherwise see if it's syntactically valid to add to our command buffer. */
			strcpy(new_input, current_input);
			output[0] = ch;
			output[1] = '\0';
			strcat(new_input, output);
			if (parse_command(new_input, output, sizeof(output), &terminal) == 0) {
				strcpy(current_input, new_input);
				printw("\r%s", output);
				clrtoeol();
				refresh();
			}
		}
	}
}



static void usage(const char *appname) {
	fprintf(stderr, "Usage: %s [socketpath]\n", appname);
}



int main(int argc, char **argv) {
	int sockfd;
	struct sockaddr_un saddr;

	/* Check command line arguments. */
	if (argc != 1 && argc != 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Establish the socket path to connect to. */
	if (argc == 2) {
		if (strlen(argv[1]) + 1 > sizeof(saddr.sun_path)) {
			errno = ENAMETOOLONG;
			perror(argv[0]);
			return EXIT_FAILURE;
		}
		strcpy(saddr.sun_path, argv[1]);
	} else {
		if (sockpath_set_default(&saddr) < 0) {
			perror(argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* Create a socket and lock its target. */
	sockfd = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	saddr.sun_family = AF_UNIX;
	if (connect(sockfd, (const struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Initialize curses. */
	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, 0);
	keypad(stdscr, 1);

	/* Run the application. */
	return run(argv[0], sockfd);
}

