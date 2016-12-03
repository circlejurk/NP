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

const char usage[] = "Usage: ./server <port>\n";

int main (int argc, char **argv)
{
	int			msock, ssock, port, fd, nfds = getdtablesize ();
	fd_set			rfds, afds;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	struct sockaddr_in	cli_addr = {0};
	User			users[MAX_USERS] = {0};

	initialize ();	/* server initialization */

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
				write (ssock, "The server is full.\n"
					"Please try again later...\n", 46);
				close (ssock);
			} else {
				FD_SET (ssock, &afds);		/* add an active socket */
				add_user (ssock, &cli_addr, users);	/* add a user */
				write (ssock, prompt, strlen(prompt));	/* show the prompt */
			}
		}
		/* execute the command input by clients */
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
	save_fds (stdfd);	/* save the fds for the server fds, e.g. console */
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
	broadcast (msg, users);
	write (sock, msg, strlen (msg));	/* since the connection has been set to 0 */
	close (sock);

	/* clear the user entry */
	clear_nps (sock, users);	/* free the allocated spaces of numbered pipes */
	clear_ups (sock, users);	/* free the allocated spaces of user pipes */
	clear_env (sock, users);	/* clear the environment variables of the user */
	memset (&users[sock - 4], 0, sizeof (User));
}

void add_user (int sock, struct sockaddr_in *cli_addr, User *users)
{
	char	msg[MAX_MSG_SIZE + 1];

	/* initialize the user entry */
	strcpy (users[sock - 4].name, "(no name)");			/* set name */
	strcpy (users[sock - 4].ip, inet_ntoa (cli_addr->sin_addr));	/* set ip */
	users[sock - 4].port = cli_addr->sin_port;			/* set port */
	users[sock - 4].connection = 1;					/* set connection */
	users[sock - 4].np = NULL;					/* set numbered pipes */
	users[sock - 4].up = calloc (MAX_USERS, sizeof (int));		/* allocate user pipes */
	init_env (sock, users);						/* initialize the environment variables of the user */
	write (sock, motd, strlen(motd));	/* print the welcome message */

	/* broadcast that you're in */
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n", users[sock - 4].name, users[sock - 4].ip, users[sock - 4].port);
	broadcast (msg, users);
}

void initialize (void)
{
	char	*wd = malloc (strlen (base_dir) + 5);
	/* initialize the original directory */
	strcpy (wd, base_dir);
	strcat (wd, "/rwg");
	chdir (wd);
	free (wd);
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

void broadcast (char *msg, User *users)
{
	int	idx;
	for (idx = 0; idx < MAX_USERS; ++idx) {
		/* broadcast to on-line clients */
		if (users[idx].connection > 0) {
			write (idx + 4, msg, strlen(msg));	/* print the message out */
		}
	}
}

void init_env (int sock, User *users)
{
	/* initialize the environment variables */
	users[sock - 4].env = malloc (2 * sizeof (char *));
	users[sock - 4].env[0] = malloc (sizeof ("PATH=bin:."));
	users[sock - 4].env[1] = NULL;
	strcpy (users[sock - 4].env[0], "PATH=bin:.");
}

void clear_env (int sock, User *users)
{
	if (users[sock - 4].env) {
		int	i;
		for (i = 0; users[sock - 4].env[i] != NULL; ++i)
			free (users[sock - 4].env[i]);
		free (users[sock - 4].env);
		users[sock - 4].env = NULL;
	}
}
