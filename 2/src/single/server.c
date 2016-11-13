#include <string.h>
#include <strings.h>
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

int main (void)
{
	int			msock, ssock, fd, nfds = getdtablesize ();
	fd_set			rfds, afds;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	struct sockaddr_in	cli_addr = {0};
	User			users[MAX_USERS] = {0};

	initialize ();	/* server initialization */

	if ((msock = passiveTCP (SERV_TCP_PORT, 0)) < 0) {	/* build a TCP connection */
		fputs ("server error: passiveTCP failed\n", stderr);
		return -1;
	}

	/* initialize the active file descriptor set */
	FD_ZERO (&afds);
	FD_SET (msock, &afds);

	while (1) {
		/* copy the active fds into read fds */
		memcpy (&rfds, &afds, sizeof(rfds));
		/* select from the rfds */
		if (select (nfds, &rfds, NULL, NULL, NULL) < 0) {
			fputs ("server error: select failed\n", stderr);
			return -1;
		}
		/* check for new connection requests */
		if (FD_ISSET (msock, &rfds)) {
			ssock = accept (msock, (struct sockaddr *)&cli_addr, &clilen);
			if (ssock < 0) {
				fputs ("server error: accept failed\n", stderr);
				return -1;
			} else if (ssock >= MAX_USERS + 4) {
				write (ssock, "It's full now...\nYou may try again after a while.\n", 50);
				close (ssock);
			} else {
				FD_SET (ssock, &afds);		/* add an active socket */
				add_user (ssock, &cli_addr, users);	/* add a user */
			}
		}
		/* handle one line command if needed */
		for (fd = 4; fd < nfds; ++fd) {
			if (fd != msock && FD_ISSET (fd, &rfds)) {
				execute (fd, users);
				if (users[fd - 4].connection < 0) {
					return -1;	/* when children exec failed */
				} else if (users[fd - 4].connection == 0) {
					FD_CLR (fd, &afds);	/* remove the inactive socket */
					rm_user (fd, users);	/* remove the user */
				} else {
					write (fd, prompt, strlen(prompt));	/* show the prompt */
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
	shell (sock, users);
	restore_fds (stdfd);
}

void rm_user (int sock, User *users)
{
	char	msg[MAX_MSG_SIZE + 1];
	/* broadcast that you're out */
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n", users[sock - 4].name);
	broadcast (msg, sock, users);
	/* clear the user entry */
	close (sock);
	users[sock - 4].name[0] = 0;
	users[sock - 4].ip[0] = 0;
	users[sock - 4].port = 0;
	users[sock - 4].connection = 0;
	clear_nps (sock, users);	/* free the allocated spaces of numbered pipes */
	clear_ups (sock, users);	/* free the allocated spaces of user pipes */
}

void add_user (int sock, struct sockaddr_in *cli_addr, User *users)
{
	char	msg[MAX_MSG_SIZE + 1];
	/* initialize the user entry */
	strcpy (users[sock - 4].name, "(no name)");
	strcpy (users[sock - 4].ip, inet_ntoa (cli_addr->sin_addr));
	users[sock - 4].port = cli_addr->sin_port;
	users[sock - 4].connection = 1;
	users[sock - 4].np = NULL;
	users[sock - 4].up = calloc (MAX_USERS, sizeof (int));
	write (sock, motd, strlen(motd));	/* print the welcome message */
	/* broadcast that you're in */
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n", users[sock - 4].name, users[sock - 4].ip, users[sock - 4].port);
	broadcast (msg, sock, users);
	write (sock, prompt, strlen(prompt));	/* show the prompt */
}

void initialize (void)
{
	/* initialize the original directory */
	/*chdir ("/u/cs/103/0310004/rwg");*/
	/* initialize the environment variables */
	clearenv ();
	putenv ("PATH=bin:.");
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

/* broadcast will not write to the client itself */
void broadcast (char *msg, int sock, User *users)
{
	int	idx;
	for (idx = 0; idx < MAX_USERS; ++idx) {
		if (users[idx].connection > 0) {
			write (idx + 4, msg, strlen(msg));	/* print the message out */
			if (idx + 4 != sock)
				write (idx + 4, prompt, strlen(prompt))	/* show the prompt */;
		}
	}
}
