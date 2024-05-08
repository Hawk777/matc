#include <curses.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "commands.h"
#include "../shared/sockpath.h"
#include "../shared/sockaddr_union.h"



static char current_input[1024] = "";

static WINDOW *chatwin, *inputwin;



static void safe_endwin(void) {
	int saved_errno = errno;
	endwin();
	errno = saved_errno;
}



static bool authenticate(int sockfd) {
	struct iovec iov = {
		.iov_base = "MATC 1",
		.iov_len = strlen("MATC 1"),
	};

	alignas (struct cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(struct ucred))];

	struct msghdr msg = {
		.msg_name = nullptr,
		.msg_namelen = 0,
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = cmsgbuf,
		.msg_controllen = sizeof(cmsgbuf),
		.msg_flags = 0,
	};

	CMSG_FIRSTHDR(&msg)->cmsg_level = SOL_SOCKET;
	CMSG_FIRSTHDR(&msg)->cmsg_type = SCM_CREDENTIALS;
	CMSG_FIRSTHDR(&msg)->cmsg_len = CMSG_LEN(sizeof(struct ucred));
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->pid = getpid();
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->uid = getuid();
	((struct ucred *) CMSG_DATA(CMSG_FIRSTHDR(&msg)))->gid = getgid();

	if (sendmsg(sockfd, &msg, MSG_NOSIGNAL | MSG_EOR) < 0)
		return false;

	char buffer[1024];
	ssize_t ret = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
	if (ret < 0)
		return false;
	if (ret == 0) {
		errno = ECONNRESET;
		return false;
	}
	buffer[ret] = '\0';
	if (strcmp(buffer, "MATC VERSION") == 0) {
		errno = EPROTONOSUPPORT;
		return false;
	} else if (strcmp(buffer, "MATC ACCESS") == 0) {
		errno = EACCES;
		return false;
	} else if (strcmp(buffer, "MATC OK") != 0) {
		errno = EPROTONOSUPPORT;
		return false;
	}

	return true;
}



static bool run_stdin_one(const char *appname, int sockfd, int *exitcode) {
	/* Get a character. */
	int ch = wgetch(inputwin);

	/* See what it is. */
	if (ch == 4) {
		/* Control-D -> terminate */
		endwin();
		*exitcode = EXIT_SUCCESS;
		return false;
	} else if (ch == 12) {
		/* Control-L -> refresh screen -> send immediately */
		char output = 12;
		if (send(sockfd, &output, 1, MSG_EOR) < 0) {
			safe_endwin();
			perror(appname);
			*exitcode = EXIT_FAILURE;
			return false;
		}
	} else if (ch == ' ' && current_input[0] != '/') {
		/* Space -> could be used at the termination of the game -> send immediately */
		char output = ' ';
		if (send(sockfd, &output, 1, MSG_EOR) < 0) {
			safe_endwin();
			perror(appname);
			*exitcode = EXIT_FAILURE;
			return false;
		}
	} else if (ch == '\r' || ch == KEY_ENTER) {
		/* Enter -> send only if our current input is terminal */
		char output[2048];
		bool terminal;
		if (parse_command(current_input, output, sizeof(output), &terminal) && terminal) {
			/* Don't send the newline for chat messages. */
			if (current_input[0] != '/')
				strcat(current_input, "\n");
			if (send(sockfd, current_input, strlen(current_input), MSG_EOR) < 0) {
				safe_endwin();
				perror(appname);
				*exitcode = EXIT_FAILURE;
				return false;
			}
			current_input[0] = '\0';
			wprintw(inputwin, "\r");
			wclrtoeol(inputwin);
			wrefresh(inputwin);
		}
	} else if (ch == KEY_BACKSPACE || ch == 8 || ch == 127) {
		/* Backspace -> if current input nonempty then remove last char */
		if (current_input[0] != '\0') {
			current_input[strlen(current_input) - 1] = '\0';
			char output[2048];
			bool terminal;
			parse_command(current_input, output, sizeof(output), &terminal);
			wprintw(inputwin, "\r%s", output);
			wclrtoeol(inputwin);
			wrefresh(inputwin);
		}
	} else if (ch == 27) {
		/* Escape -> clear the current input buffer */
		current_input[0] = '\0';
		waddstr(inputwin, "\r");
		wclrtoeol(inputwin);
		wrefresh(inputwin);
	} else if (ch != ERR && strlen(current_input) + 2 < sizeof(current_input)) {
		/* Otherwise see if it's syntactically valid to add to our command buffer. */
		char new_input[1024];
		strcpy(new_input, current_input);
		char output[2048];
		output[0] = ch;
		output[1] = '\0';
		strcat(new_input, output);
		bool terminal;
		if (parse_command(new_input, output, sizeof(output), &terminal)) {
			strcpy(current_input, new_input);
			waddstr(inputwin, "\r");
			waddstr(inputwin, output);
			wclrtoeol(inputwin);
			wrefresh(inputwin);
		}
	}

	return true;
}



static bool run_socket_one(const char *appname, int sockfd, int *exitcode) {
	/* Read the packet from the socket. */
	char buffer[1024];
	ssize_t ret = read(sockfd, buffer, sizeof(buffer));
	if (ret < 0) {
		safe_endwin();
		perror(appname);
		*exitcode = EXIT_FAILURE;
		return false;
	} else if (ret == 0) {
		endwin();
		*exitcode = EXIT_SUCCESS;
		return false;
	}
	buffer[ret] = '\0';

	/* Output the message. */
	waddstr(chatwin, buffer);
	waddstr(chatwin, "\n");
	wrefresh(chatwin);
	return true;
}



static int run(const char *appname, int sockfd) {
	for (;;) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(0 /* stdin */, &rfds);
		FD_SET(sockfd, &rfds);
		if (select(sockfd + 1, &rfds, nullptr, nullptr, nullptr) < 0) {
			safe_endwin();
			perror(appname);
			return EXIT_FAILURE;
		}

		if (FD_ISSET(0, &rfds)) {
			int exitcode;
			if (!run_stdin_one(appname, sockfd, &exitcode))
				return exitcode;
		}
		if (FD_ISSET(sockfd, &rfds)) {
			int exitcode;
			if (!run_socket_one(appname, sockfd, &exitcode))
				return exitcode;
		}
	}
}



static void usage(const char *appname) {
	fprintf(stderr, "Usage: %s [socketpath]\n", appname);
}



int main(int argc, char **argv) {
	/* Check command line arguments. */
	if (argc != 1 && argc != 2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Establish the socket path to connect to. */
	union sockaddr_union saddr;
	if (argc == 2) {
		if (strlen(argv[1]) + 1 > sizeof(saddr.sun.sun_path)) {
			errno = ENAMETOOLONG;
			perror(argv[0]);
			return EXIT_FAILURE;
		}
		strcpy(saddr.sun.sun_path, argv[1]);
	} else {
		if (!sockpath_set_default(&saddr.sun)) {
			perror(argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* Create a socket and connect to the server. */
	int sockfd = socket(PF_UNIX, SOCK_SEQPACKET, 0);
	if (sockfd < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}
	saddr.sun.sun_family = AF_UNIX;
	if (connect(sockfd, &saddr.s, sizeof(saddr)) < 0) {
		perror(argv[0]);
		return EXIT_FAILURE;
	}

	/* Transmit an authentication/version-negotiation packet. */
	if (!authenticate(sockfd)) {
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
	timeout(0);
	chatwin = newwin(LINES - 1, 0, 0, 0);
	inputwin = newwin(0, 0, LINES - 1, 0);
	scrollok(chatwin, 1);
	idlok(chatwin, 1);

	/* Run the application. */
	return run(argv[0], sockfd);
}

