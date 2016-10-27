#include <stdio.h>
#include <stdlib.h>
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

#define SERV_TCP_PORT	9527

int shell (void);
void reaper (int sig);

int main (void)
{
	int			sockfd, newsockfd;
	socklen_t		clilen;
	pid_t			childpid;
	struct sockaddr_in	cli_addr, serv_addr;

	chdir ("/u/cs/103/0310004/ras");

	/* establish a signal handler for SIGCHLD */
	signal (SIGCHLD, reaper);

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("server error: cannot open socket\n", stderr);
		exit (1);
	}

	/* set up server socket addr */
	bzero (&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (SERV_TCP_PORT);

	/* bind to server address */
	if (bind (sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("server error: cannot bind local address\n", stderr);
		exit (1);
	}

	/* listen for requests */
	if (listen (sockfd, 0) < 0) {
		fputs ("server error: listen failed\n", stderr);
		exit (1);
	}

	while (1) {
		/* accept connection request */
		clilen = sizeof(cli_addr);
		newsockfd = accept (sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (newsockfd < 0) {
			if (errno == EINTR)
				continue;
			fputs ("server error: accept failed\n", stderr);
			exit (1);
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("server error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			close (STDIN_FILENO);
			close (STDOUT_FILENO);
			close (STDERR_FILENO);
			dup (newsockfd);
			dup (newsockfd);
			dup (newsockfd);
			close (sockfd);
			close (newsockfd);
			exit (shell ());
		}
		close (newsockfd);
	}

	return 0;
}

void reaper (int sig)
{
	while (waitpid (-1, NULL, WNOHANG) > 0);
	signal (sig, reaper);
}
