#include <strings.h>
#include <string.h>
#include <stdint.h>
#define __USE_ISOC99
#include <stdio.h>
#undef __USE_ISOC99
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
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
void rm_user (int sock, User *users);
void execute (int sock, User *users);
void broadcast (char *msg, User *users);

int main (void)
{
	int			msock, ssock, fd, nfds = getdtablesize ();
	fd_set			rfds, afds;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	struct sockaddr_in	cli_addr = {0};
	User			users[MAX_USERS + 1] = {0};

	initialize ();	/* server initialization */

	if ((msock = passiveTCP (SERV_TCP_PORT, 0)) < 0)	/* build a TCP connection */
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
					return -1;	/* when children exec failed */
				} else if (users[fd].connection == 0) {
					FD_CLR (fd, &afds);	/* remove the inactive socket */
					rm_user (fd, users);	/* remove the user */
				}
			}
		}
	}

	return 0;
}

void execute (int sock, User *users)
{
	int	stdfd[3];
	save_fds (stdfd);
	dup2 (sock, STDIN_FILENO);
	dup2 (sock, STDOUT_FILENO);
	dup2 (sock, STDERR_FILENO);
	shell (users + sock);
	restore_fds (stdfd);
	if (sock == 1) {
		dup2 (2, 0);
		close (2);
	}
	if (users[sock].connection > 0)
		write (sock, prompt, strlen(prompt));	/* show the prompt */
}

void broadcast (char *msg, User *users)
{
	int	fd;
	for (fd = 1; fd <= MAX_USERS; ++fd) {
		if (users[fd].connection > 0) {
			write (fd, msg, strlen(msg));	/* print the message out */
			write (fd, prompt, strlen(prompt));	/* show the prompt */
		}
	}
}

void rm_user (int sock, User *users)
{
	char	msg[MAX_MSG_SIZE + 1];
	/* broadcast that you're out */
	snprintf (msg, MAX_MSG_SIZE + 1, "\n*** User '%s' left. ***\n", users[sock].name);
	broadcast (msg, users);
	/* clear the user entry */
	close (sock);
	users[sock].name[0] = 0;
	users[sock].ip[0] = 0;
	users[sock].port = 0;
	users[sock].connection = 0;
	clear_nps (users[sock].np);	/* free the allocated space of numbered pipes */
	users[sock].np->fd = NULL;
}

void add_user (int sock, struct sockaddr_in *cli_addr, User *users)
{
	char	msg[MAX_MSG_SIZE + 1];
	/* initialize the user entry */
	strcpy (users[sock].name, "(no name)");
	strcpy (users[sock].ip, inet_ntoa (cli_addr->sin_addr));
	users[sock].port = cli_addr->sin_port;
	users[sock].connection = 1;
	write (sock, motd, strlen(motd));	/* print the welcome message */
	/* broadcast that you're in */
	snprintf (msg, MAX_MSG_SIZE + 1, "\n*** User '%s' entered from %s/%d. ***\n", users[sock].name, users[sock].ip, users[sock].port);
	broadcast (msg, users);
}

void initialize (void)
{
	/* initialize the original directory */
	/*chdir ("/u/cs/103/0310004/rwg");*/
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
