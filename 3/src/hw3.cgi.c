#include <string.h>
#include <stdint.h>
#define __USE_ISOC99
#define __USE_POSIX
#include <stdio.h>
#undef __USE_ISOC99
#undef __USE_POSIX
#include <stdlib.h>
#include <bits/time.h>
#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <unistd.h>
#undef __USE_MISC
#include <fcntl.h>
#include <sys/select.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_BUF_SIZE 3000

typedef struct host {
	struct sockaddr_in	sin;	/* the server socket address */
	int			sock;	/* the connection socket */
	FILE			*fin;	/* the batch file for host server input */
	int			stat;	/* 0: not connected */
					/* 1: connected but not writable */
					/* 2: connected and writable */
} Host;

int resolv_requests (Host *hosts);
int add_host (Host *host, char *hostname, char *port, char *filename);
void rm_host (Host *host);
void preoutput (Host *hosts);
void postoutput (void);
int try_connect (Host *hosts);
void output (char *msg, int idx);
int receive (Host *hosts, int idx);
int contain_prompt (char *s);
int send_cmd (Host *hosts, int idx);

const char	prompt[] = "% ";
fd_set		rfds, afds;

int main (void)
{
	int		i, nfds = getdtablesize ();
	Host		hosts[5] = {0};
	struct timeval	timeout;

	FD_ZERO (&afds);

	/* resolve the requests and set up the hosts */
	if (resolv_requests (hosts) < 0)
		return -1;

	preoutput (hosts);

	while (hosts[0].sock + hosts[1].sock + hosts[2].sock + hosts[3].sock + hosts[4].sock) {
		/* try to connect to servers */
		if (try_connect (hosts) < 0)
			return -1;

		/* copy the active fds into read fds */
		memcpy (&rfds, &afds, sizeof(rfds));
		/* select timeout for 10 ms */
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		/* select from the rfds */
		if (select (nfds, &rfds, NULL, NULL, &timeout) < 0) {
			fputs ("error: select failed\n", stderr);
			return -1;
		}

		for (i = 0; i < 5; ++i) {
			if (hosts[i].stat == 0)
				continue;
			/* receive messages from servers */
			if (FD_ISSET (hosts[i].sock, &rfds)) {
				if (receive (hosts, i) < 0)
					return -1;
			}
			/* send a command to servers */
			if (hosts[i].stat == 2 && FD_ISSET (fileno (hosts[i].fin), &rfds)) {
				if (send_cmd (hosts, i) < 0)
					return -1;
			}
		}

		usleep (1000);	/* sleep for 1 ms */
	}

	postoutput ();

	return 0;
}

int send_cmd (Host *hosts, int idx)
{
	char	cmd[MAX_BUF_SIZE + 1], *p, *html_msg;

	p = fgets (cmd, MAX_BUF_SIZE + 1, hosts[idx].fin);
	if (ferror (hosts[idx].fin)) {			/* error */
		fputs ("error: fgets failed when reading commands\n", stderr);
		return -1;
	} else if (feof (hosts[idx].fin) && p == NULL) {	/* EOF */
		write (hosts[idx].sock, "exit\n", 5);
	} else {
		write (hosts[idx].sock, cmd, strlen (cmd));
		strtok (cmd, "\r\n");
		html_msg = malloc (strlen (cmd) + 12);
		strcpy (html_msg, "<b>");
		strcat (html_msg, cmd);
		strcat (html_msg, "</b><br>");
		output (html_msg, idx);
		free (html_msg);
	}
	hosts[idx].stat = 1;

	return 0;
}

int receive (Host *hosts, int idx)
{
	int	len;
	char	buf[MAX_BUF_SIZE + 1], *token, *c, *html_msg;

	if ((len = read (hosts[idx].sock, buf, MAX_BUF_SIZE)) < 0) {
		fprintf (stderr, "error: failed to read from hosts[%d]\n", idx);
		return -1;
	} else if (len == 0) {
		/* close the connection to the host */
		rm_host (&hosts[idx]);
	} else {
		buf[len] = 0;	/* let the string be null-terminated */
		/* print the received messages back to the user */
		token = buf;
		while (token[0] != 0) {
			for (c = token; *c != '\n' && *c != 0; ++c);
			if (*c == '\n') {
				*c = 0;
				++c;
			}

			if (contain_prompt (token))
				hosts[idx].stat = 2;
			html_msg = malloc (strlen (token) + 5);
			strcpy (html_msg, token);
			if (strcmp (token, prompt) != 0)
				strncat (html_msg, "<br>", 5);
			output (html_msg, idx);
			free (html_msg);

			token = c;
		}
	}

	return len;
}

void rm_host (Host *host)
{
	if (host->sock != 0) {
		if (host->stat) {
			FD_CLR (host->sock, &afds);
			close (host->sock);
		}
		FD_CLR (fileno (host->fin), &afds);
		fclose (host->fin);
		memset (host, 0, sizeof (Host));
	}
}

int contain_prompt (char *s)
{
	int	i = 0;
	while (*s != 0) {
		if (*s == prompt[i]) {
			if (++i == strlen (prompt))
				return 1;
		} else
			i = 0;
		++s;
	}
	return 0;
}

void postoutput (void)
{
	write (STDOUT_FILENO, "</font>\n", 8);
	write (STDOUT_FILENO, "</body>\n", 8);
	write (STDOUT_FILENO, "</html>\n", 8);
}

void output (char *msg, int idx)
{
	char	buf[MAX_BUF_SIZE + 1] = {0};
	snprintf (buf, MAX_BUF_SIZE + 1, "<script>document.all['m%d'].innerHTML += \"", idx);
	strncat (buf, msg, MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "\";</script>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	write (STDOUT_FILENO, buf, strlen (buf));
}

int try_connect (Host *hosts)
{
	int	i;
	for (i = 0; i < 5; ++i) {
		if (hosts[i].sock && hosts[i].stat == 0) {
			if (connect (hosts[i].sock, (struct sockaddr *) &hosts[i].sin, sizeof (hosts[i].sin)) < 0) {
				if (errno != EINPROGRESS && errno != EALREADY) {
					fprintf (stderr, "error: connect failed at hosts[%d]\n", i);
					fprintf (stderr, "errno: %d %s\n", errno, strerror (errno));
					return -1;
				}
			} else {
				FD_SET (hosts[i].sock, &afds);
				hosts[i].stat = 1;
			}
		}
	}
	return 0;
}

void preoutput (Host *hosts)
{
	int	i;
	char	buf[MAX_BUF_SIZE + 1] = {0}, tmp[64];
	strncpy (buf, "Content-Type: text/html\n\n", MAX_BUF_SIZE + 1);
	strncat (buf, "<html>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<head>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<title>Network Programming Homework 3</title>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "</head>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<body bgcolor=#336699>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<font face=\"Courier New\" size=2 color=#FFFF99>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<table width=\"800\" border=\"1\">\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	for (i = 0; i < 5; ++i) {
		strncat (buf, "<td>", MAX_BUF_SIZE + 1 - strlen (buf));
		if (hosts[i].sock)
			strncat (buf, inet_ntoa (hosts[i].sin.sin_addr), MAX_BUF_SIZE + 1 - strlen (buf));
		strncat (buf, "</td>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	}
	strncat (buf, "</tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	for (i = 0; i < 5; ++i) {
		snprintf (tmp, 64, "<td valign=\"top\" id=\"m%d\"></td>\n", i);
		strncat (buf, tmp, MAX_BUF_SIZE + 1 - strlen (buf));
	}
	strncat (buf, "</tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "</table>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	write (STDOUT_FILENO, buf, sizeof (buf));
}

int resolv_requests (Host *hosts)
{
	int	i;
	char	*qstring;		/* QUERY_STRING */
	char	*method;		/* REQUEST_METHOD */
	char	*content_len;		/* CONTENT_LENGTH */
	char	*token, hostname[64], port[10], filename[32];

	method = getenv ("REQUEST_METHOD");
	content_len = getenv ("CONTENT_LENGTH");

	/* construct the query string according to the request method */
	if (strcmp (method, "GET") == 0) {
		qstring = malloc (strlen (getenv ("QUERY_STRING")) + 1);
		strcpy (qstring, getenv ("QUERY_STRING"));
	} else if (strcmp (method, "POST") == 0) {
		qstring = malloc (atoi (content_len) + 1);
		fgets (qstring, atoi (content_len) + 1, stdin);
		/*fputs ("Sorry, the POST method is currently not supported.\n", stdout);*/
		/*return -1;*/
	} else {
		fputs ("What's the matter with you?\n", stdout);
		return -1;
	}

	token = strtok (qstring, "&");
	while (token != NULL) {
		/* reset to empty string */
		hostname[0] = port[0] = filename[0] = 0;
		/* read the hostname or IP address */
		sscanf (token, "h%d=%s", &i, hostname);
		token = strtok (NULL, "&");
		/* read the port of the service */
		sscanf (token, "p%d=%s", &i, port);
		token = strtok (NULL, "&");
		/* read the filename of the batch input */
		sscanf (token, "f%d=%s", &i, filename);
		token = strtok (NULL, "&");

		/* check input validity */
		if (*hostname == 0 || *port == 0 || *filename == 0)
			continue;

		if (add_host (&hosts[i - 1], hostname, port, filename) < 0) {
			free (qstring);
			fprintf (stderr, "error: add_host failed at hosts[%d]\n", i - 1);
			return -1;
		}

	}

	free (qstring);
	return 0;
}

int add_host (Host *host, char *hostname, char *port, char *filename)
{
	struct hostent		*phe;

	/* set up server socket addr */
	memset (&host->sin, 0, sizeof (host->sin));
	host->sin.sin_family = AF_INET;
	host->sin.sin_port = htons ((uint16_t) atoi (port));
	if ((phe = gethostbyname (hostname)))
		host->sin.sin_addr = *((struct in_addr *) phe->h_addr_list[0]);
	else if ((host->sin.sin_addr.s_addr = inet_addr (hostname)) == INADDR_NONE) {
		fprintf (stderr, "error: cannot get the hostname '%s'\n", hostname);
		return -1;
	}

	/* allocate the socket */
	if ((host->sock = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("error: failed to build a socket\n", stderr);
		return -1;
	}
	fcntl(host->sock, F_SETFL, O_NONBLOCK);	/* set the socket as non-blocking mode */

	/* open the input file */
	if ((host->fin = fopen (filename, "r")) == NULL) {
		fprintf (stderr, "error: failed to open file '%s'\n", filename);
		return -1;
	}
	FD_SET (fileno (host->fin), &afds);	/* add the input file to the active fds for select */

	/* set the status of the host to 'not connected' */
	host->stat = 0;

	return 0;
}
