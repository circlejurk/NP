#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define __USE_POSIX
#include <signal.h>
#undef __USE_POSIX
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __USE_MISC
#include <sys/shm.h>
#undef __USE_MISC

#include "shm.h"

static void sig_handler (int sig);
int passiveTCP (int port, int qlen);
void create_shm (void);

const char usage[] = "Usage: ./server <port>\n";

int main (int argc, char **argv)
{
	int			msock, ssock, port;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	pid_t			childpid;
	struct sockaddr_in	cli_addr;

	/* establish signal handlers */
	signal (SIGCHLD, sig_handler);
	signal (SIGINT, sig_handler);
	signal (SIGQUIT, sig_handler);
	signal (SIGTERM, sig_handler);

	/* setting up the port number */
	if (argc != 2 || ! isnumber (argv[1])) {
		fputs (usage, stderr);
		return -1;
	} else {
		port = atoi (argv[1]);
	}

	/* build a TCP connection */
	if ((msock = passiveTCP (port, 0)) < 0) {
		fputs ("server error: passiveTCP failed\n", stderr);
		return -1;
	}

	/* create the shared memory for clients */
	create_shm ();

	while (1) {
		/* accept connection request */
		ssock = accept (msock, (struct sockaddr *)&cli_addr, &clilen);
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			fputs ("server error: accept failed\n", stderr);
			return -1;
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("server error: fork failed\n", stderr);
			return -1;
		} else if (childpid == 0) {
			dup2 (ssock, STDIN_FILENO);
			dup2 (ssock, STDOUT_FILENO);
			dup2 (ssock, STDERR_FILENO);
			close (msock);
			close (ssock);
			exit (shell (&cli_addr));
		}
		close (ssock);
	}

	return 0;
}

void sig_handler (int sig)
{
	if (sig == SIGCHLD) {
		while (waitpid (-1, NULL, WNOHANG) > 0);
	} else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
		int	shmid;
		if ((shmid = shmget (SHMKEY, MAX_USERS * sizeof (User), PERM)) < 0) {
			fputs ("server error: shmget failed\n", stderr);
			exit (1);
		}
		if (shmctl (shmid, IPC_RMID, NULL) < 0) {
			fputs ("server error: shmctl IPC_RMID failed\n", stderr);
			exit (1);
		}
		exit (0);
	}
	signal (sig, sig_handler);
}

int passiveTCP (int port, int qlen)
{
	int			sockfd;
	struct sockaddr_in	serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("server error: cannot open socket\n", stderr);
		return -1;
	}

	/* set up server socket addr */
	bzero (&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (port);

	/* bind to server address */
	if (bind (sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("server error: cannot bind local address\n", stderr);
		return -1;
	}

	/* listen for requests */
	if (listen (sockfd, qlen) < 0) {
		fputs ("server error: listen failed\n", stderr);
		return -1;
	}

	return sockfd;
}

void create_shm (void)
{
	int	shmid = 0;
	User	*users = NULL;
	/* allocate the shared memory for users () */
	if ((shmid = shmget (SHMKEY, MAX_USERS * sizeof (User), PERM | IPC_CREAT)) < 0) {
		fputs ("server error: shmget failed\n", stderr);
		exit (1);
	}
	/* attach the allocated shared memory */
	if ((users = (User *) shmat (shmid, NULL, 0)) == (User *) -1) {
		fputs ("server error: shmat failed\n", stderr);
		exit (1);
	}
	/* set the allocated shared memory segment to zero */
	memset (users, 0, MAX_USERS * sizeof (User));
	/* detach the shared memory segment */
	shmdt (users);
}
