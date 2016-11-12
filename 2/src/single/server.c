#include <stdio.h>
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
#include <strings.h>
#include <string.h>
#include <stdint.h>
#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <unistd.h>
#undef __USE_MISC
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "single.h"

void initialize (void);
int passiveTCP (int port, int qlen);
void add_user (int sock, struct sockaddr_in *cli_addr, User *users);
void rm_user (User *users);
void execute (int fd, User *users);

int main (void)
{
	int			msock, ssock, fd, nfds = getdtablesize ();
	fd_set			rfds, afds;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	struct sockaddr_in	cli_addr = {0};
	User			users[MAX_USERS + 1] = {0};

	/* build a TCP connection */
	if ((msock = passiveTCP (SERV_TCP_PORT, 0)) < 0)
		return -1;

	/* initialize the active file descriptor set */
	FD_ZERO (&afds);
	FD_SET (msock, &afds);

	while (1) {
		/* copy the active fds into read fds */
		memcpy (&rfds, &afds, sizeof(rfds));
		/* select from the rfds */
		if (select (nfds, &rfds, NULL, NULL, NULL) < 0)
			return -1;
		/* check for new connection requests */
		if (FD_ISSET (msock, &rfds)) {
			ssock = accept (msock, (struct sockaddr *)&cli_addr, &clilen);
			if (ssock < 0)
				return -1;
			FD_SET (ssock, &afds);		/* add an active socket */
			add_user (ssock, &cli_addr, users);	/* add a user */
		}
		/* handle one line command if needed */
		for (fd = 0; fd < nfds; ++fd) {
			if (fd != msock && FD_ISSET (fd, &rfds)) {
				execute (fd, users);
				if (users[fd].connection < 0) {
					return -1;
				} else if (users[fd].connection == 0) {
					FD_CLR (fd, &afds);	/* remove the inactive socket */
					rm_user (users + fd);	/* remove the user */
				}
			}
		}
	}

	return 0;
}

void execute (int fd, User *users)
{
	int	stdfd[3];
	save_fds (stdfd);
	dup2 (fd, STDIN_FILENO);
	dup2 (fd, STDOUT_FILENO);
	dup2 (fd, STDERR_FILENO);
	shell (users + fd);
	restore_fds (stdfd);
	if (fd == 1) {
		dup2 (2, 0);
		close (2);
	}
	write (fd, prompt, strlen(prompt));	/* show the prompt */
}

void rm_user (User *user)
{
	close (user->sock);
	user->sock = 0;
	user->name[0] = 0;
	user->ip[0] = 0;
	user->port = 0;
	user->connection = 0;
	clear_nps (user->np);	/* free the allocated space of numbered pipes */
	user->np->fd = NULL;
	/* broadcast that you're fucking out */
}

void add_user (int sock, struct sockaddr_in *cli_addr, User *users)
{
	users[sock].sock = sock;
	strcpy (users[sock].name, "no name");
	strcpy (users[sock].ip, inet_ntoa (cli_addr->sin_addr));
	users[sock].port = cli_addr->sin_port;
	users[sock].connection = 1;

	write (sock, motd, strlen(motd));	/* print the welcome message */
	write (sock, prompt, strlen(prompt));	/* show the prompt */
	/* broadcast that you're fucking in */
}

void initialize (void)
{
	/* initialize the original directory */
	chdir ("/u/cs/103/0310004/rwg");
	/* initialize the environment variables */
	clearenv ();
	putenv ("PATH=bin:.");
	/* close the original fds */
	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
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
