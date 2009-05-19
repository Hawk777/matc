#include "atcproc.h"
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>



/* The process ID of the running ATC process, or -1 if none currently running. */
static volatile pid_t child_pid = -1;

/* The write end of the data pipe, or -1 if none currently open. */
static volatile int pipe_write = -1;

/* The callback function. */
static void (*volatile child_death_callback)(void);



/* A signal handler to handle SIGINT/SIGTERM sent to the parent. */
static void term_sig_handler(int signum __attribute__((unused))) {
	/* Kill the child. */
	atcproc_stop();

	/* Die. */
	_exit(EXIT_SUCCESS);
}

/* A signal handler to handle SIGCHLD sent to the parent. */
static void child_sig_handler(int signum __attribute__((unused))) {
	pid_t pid;
	int status;

	/* Fetch and clear child PID. */
	pid = child_pid;
	child_pid = -1;

	/* Close and clear pipe write FD. */
	if (pipe_write != -1)
		close(pipe_write);
	pipe_write = -1;

	/* Reap the child. */
	if (waitpid(pid, &status, 0) < 0)
		return;

	/* Call the callback. */
	if (child_death_callback)
		child_death_callback();
}



/* Blocks all signals. */
static void block_sigs(sigset_t *old_mask) {
	sigset_t sigs;
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGCHLD);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGINT);
	sigprocmask(SIG_BLOCK, &sigs, old_mask);
}

/* Restores the previous signal mask saved by block_sigs(). */
static void restore_sigs(const sigset_t *old_mask) {
	int temp_errno = errno;
	sigprocmask(SIG_SETMASK, old_mask, 0);
	errno = temp_errno;
}



int atcproc_start(const char *game) {
	int pipefds[2];
	pid_t pid;
	struct rlimit rlim;
	unsigned int i;
	struct sigaction sa;
	sigset_t saved_mask;
	int ret = -1;

	/* Block all signals to avoid race conditions. */
	block_sigs(&saved_mask);

	/* Handle SIGINT/SIGTERM. */
	sa.sa_handler = &term_sig_handler;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, 0) < 0)
		goto out;
	if (sigaction(SIGTERM, &sa, 0) < 0)
		goto out;

	/* Ignore SIGPIPE. */
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, 0) < 0)
		goto out;

	/* Install signal handler for SIGCHLD. */
	sa.sa_handler = &child_sig_handler;
	sa.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, 0) < 0)
		goto out;

	/* Check if a child process is already running. */
	if (atcproc_is_running()) {
		errno = EALREADY;
		goto out;
	}

	/* Create the pipe. */
	if (pipe(pipefds) < 0)
		goto out;

	/* Fork. */
	pid = fork();
	if (pid < 0)
		goto out;
	else if (pid == 0) {
		/* Child process. Copy the pipe reader to stdin. */
		if (dup2(pipefds[0], 0) < 0) {
			perror(0);
			exit(EXIT_FAILURE);
		}
		/* Get the number of possible FDs. */
		getrlimit(RLIMIT_NOFILE, &rlim);
		/* Close everything except stdin/stdout/stderr. */
		for (i = 3; i < rlim.rlim_cur; i++)
			close(i);
		/* Restore the signal mask (we want to leave signals unblocked in atc). */
		restore_sigs(&saved_mask);
		/* Execute ATC. */
		if (game)
			execlp("atc", "atc", "-g", game, (const char *) 0);
		else
			execlp("atc", "atc", (const char *) 0);
		/* If we got here, execlp() failed. */
		perror(0);
		exit(EXIT_FAILURE);
	} else {
		/* Parent process. Record child PID and pipe write FD. */
		child_pid = pid;
		pipe_write = pipefds[1];
		/* Close the pipe read FD. */
		close(pipefds[0]);
		/* Done! */
		ret = 0;
	}

out:
	restore_sigs(&saved_mask);
	return ret;
}



int atcproc_stop(void) {
	sigset_t saved_mask;
	int ret = -1, status;
	pid_t died_pid;
	ssize_t ssz;

	/* Block signals to avoid race conditions. */
	block_sigs(&saved_mask);

	/* Check that the child PID is valid. */
	if (child_pid < 0) {
		errno = ESRCH;
		goto out;
	}

	/* Send it SIGCONT in case it was paused. */
	kill(child_pid, SIGCONT);

	/* Send it SIGINT. */
	if (kill(child_pid, SIGINT) < 0)
		goto out;

	/* We can't reliably wait for the process to "receive" SIGINT, so just keep trying repeatedly. */
	while ((died_pid = waitpid(child_pid, &status, WNOHANG)) == 0) {
		/* Nothing died yet. Pipe in a Y to answer the "quit now?" question. */
		ssz = write(pipe_write, "y", 1);
		/* Go to sleep for a bit. */
		usleep(100000);
	}
	if (died_pid < 0)
		goto out;

	/* We have reaped the child, so clear its PID and close the pipe. The signal handler will no-op. */
	child_pid = -1;
	close(pipe_write);
	pipe_write = -1;
	ret = 0;

out:
	restore_sigs(&saved_mask);
	return ret;
}



int atcproc_pause(void) {
	sigset_t saved_mask;
	int ret = -1;

	/* Block signals to avoid race conditions. */
	block_sigs(&saved_mask);

	/* Check that the child PID is valid. */
	if (child_pid < 0) {
		errno = ESRCH;
		goto out;
	}

	/* Send it SIGSTOP. */
	if (kill(child_pid, SIGSTOP) < 0)
		goto out;

	/* Success! */
	ret = 0;

out:
	restore_sigs(&saved_mask);
	return ret;
}



int atcproc_resume(void) {
	sigset_t saved_mask;
	int ret = -1;

	/* Block signals to avoid race conditions. */
	block_sigs(&saved_mask);

	/* Check that the child PID is valid. */
	if (child_pid < 0) {
		errno = ESRCH;
		goto out;
	}

	/* Send it SIGCONT. */
	if (kill(child_pid, SIGCONT) < 0)
		goto out;

	/* Success! */
	ret = 0;

out:
	restore_sigs(&saved_mask);
	return ret;
}



int atcproc_is_running(void) {
	sigset_t saved_mask;
	int ret;

	block_sigs(&saved_mask);
	ret = child_pid != -1;
	restore_sigs(&saved_mask);
	return ret;
}



int atcproc_send(const char *string) {
	sigset_t saved_mask;
	int ret = -1;
	ssize_t written;

	/* Block signals to avoid race condition. */
	block_sigs(&saved_mask);

	/* As long as we have data left, keep trying to write it. */
	while (string[0] != '\0') {
		written = write(pipe_write, string, strlen(string));
		if (written < 0)
			goto out;
		string += written;
	}

	/* Done! */
	ret = 0;

out:
	restore_sigs(&saved_mask);
	return ret;
}



void atcproc_set_cb(void (*cb)(void)) {
	sigset_t saved_mask;

	block_sigs(&saved_mask);
	child_death_callback = cb;
	restore_sigs(&saved_mask);
}

