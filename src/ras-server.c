#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERV_TCP_PORT	9527


int shell (void);

int main (void)
{
	int			sockfd, newsockfd;
	socklen_t		clilen;
	pid_t			childpid;
	struct sockaddr_in	cli_addr, serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("server error: cannot open socket\n", stderr);
		exit (1);
	}

	/* setup server socket addr */
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
			fputs ("server error: accept failed\n", stderr);
			exit (1);
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("server error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			close (STDIN_FILENO);	close (STDOUT_FILENO);	close (STDERR_FILENO);
			dup (newsockfd);	dup (newsockfd);	dup (newsockfd);
			close (sockfd);
			close (newsockfd);
			shell ();
			exit (0);
		}
		close (newsockfd);
	}

	return 0;
}
