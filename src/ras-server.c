#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define SERV_TCP_PORT	9527
#define MAX_SINGLE_LINE	15000
#define MAX_CMD_SIZE	256

const char* prompt = "% ";
const char* motd = "****************************************\n"
		   "** Welcome to the information server. **\n"
		   "****************************************\n";

int ras ()
{
	return 0;
}


int main (void)
{
	int			sockfd, rasfd;
	socklen_t		clilen;
	pid_t			childpid;
	struct sockaddr_in	cli_addr, serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("server error: cannot open socket", stderr);
		exit (1);
	}

	/* setup server socket addr */
	bzero (&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (SERV_TCP_PORT);

	/* bind to server address */
	if (bind (sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("server error: cannot bind local address", stderr);
		exit (1);
	}

	/* listen for requests */
	if (listen (sockfd, 0) < 0) {
		fputs ("server error: listen failed", stderr);
		exit (1);
	}

	while (1) {
		/* accept connection request */
		clilen = sizeof(cli_addr);
		rasfd = accept (sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (rasfd < 0) {
			fputs ("server error: accept failed", stderr);
			exit (1);
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("server error: fork failed", stderr);
			exit (1);
		} else if (childpid == 0) {
			close (sockfd);
			ras (rasfd);
			exit (0);
		}
		close (rasfd);
	}

	return 0;
}
