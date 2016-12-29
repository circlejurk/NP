#include <string.h>
#include <inttypes.h>
#define __USE_ISOC99
#include <stdio.h>
#undef __USE_ISOC99
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
#include <ctype.h>
#define __USE_GNU
#include <unistd.h>
#undef __USE_GNU
#define __USE_POSIX
#include <signal.h>
#undef __USE_POSIX
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __USE_MISC
#include <sys/shm.h>
#undef __USE_MISC
#include <errno.h>
#include "socks.h"

Request		req = {0};
Reply		rep = {0};
const char config_file[] = "socks.conf";

int socks (struct sockaddr_in src)
{
	int	dest;	/* the destination socket */

	/* receive the SOCKS4 request from src */
	if (recv_req () < 0) {
		fputs ("error: recv_req() failed\n", stderr);
		return -1;
	}

	/* check firewall rules */
	if (!check_fw ()) {
		rep.cd = 91;	/* request rejected */
		rep.dest_port = 0;
		rep.dest_ip.s_addr = 0;
		send_reply ();
		verbose (&src);
		return 0;
	}
	rep.cd = 90;	/* request accepted */

	/* build the connection */
	switch (req.cd) {
	case 1:		/* CONNECT mode */
		rep.dest_port = req.dest_port;
		rep.dest_ip = req.dest_ip;
		if ((dest = CONNECT ()) < 0) {
			fputs ("error: CONNECT failed\n", stderr);
			rep.cd = 91;	/* request failed */
		}
		break;
	case 2:		/* BIND mode */
		rep.dest_port = BIND_PORT_SRC;
		rep.dest_ip.s_addr = 0;
		if ((dest = BIND ()) < 0) {
			fputs ("error: BIND failed\n", stderr);
			rep.cd = 91;	/* request failed */
		}
		break;
	}

	send_reply ();	/* send the SOCK4 reply */
	verbose (&src);	/* print the verbose output */

	return (rep.cd == 91) ? -1 : transmission (dest);	/* start the connection */
}

int BIND (void)
{
	return -1;
}

int CONNECT (void)
{
	int	dest;
	struct sockaddr_in	sin;

	/* set up the socket addr of the destination */
	memset (&sin, 0, sizeof (sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons (req.dest_port);
	sin.sin_addr = req.dest_ip;

	/* allocate the socket */
	if ((dest = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("error: failed to build a socket\n", stderr);
		return -1;
	}

	/* connect to the destination */
	if (connect (dest, (const struct sockaddr *) &sin, sizeof (sin)) < 0) {
		fputs ("error: failed to connect to the destination\n", stderr);
		return -1;
	}

	return dest;
}

int transmission (int dest)
{
	int	nfds = dest + 1;
	char	buf[TRANS_SIZE];
	fd_set	rfds, afds;

	/* initialize the active file descriptor set */
	FD_ZERO (&afds);
	FD_SET (dest, &afds);
	FD_SET (STDIN_FILENO, &afds);

	while (1) {
		/* copy the active fds into read fds */
		memcpy (&rfds, &afds, sizeof(rfds));
		/* select from the rfds */
		if (select (nfds, &rfds, NULL, NULL, NULL) < 0) {
			fputs ("server error: select failed\n", stderr);
			return -1;
		}
		/* read from src and write to dest */
		if (FD_ISSET (STDIN_FILENO, &rfds)) {
			read (STDIN_FILENO, buf, TRANS_SIZE);
			write (dest, buf, TRANS_SIZE);
		}
		/* read from dest and write to src */
		if (FD_ISSET (dest, &rfds)) {
			read (dest, buf, TRANS_SIZE);
			write (STDOUT_FILENO, buf, TRANS_SIZE);
		}
	}

	return 0;
}

void verbose (struct sockaddr_in *src)
{
	fprintf (stderr, "VN: %d, CD: %d, USERID: %s, DN: %s\n", req.vn, req.cd, req.user, req.dn);
	fprintf (stderr, "DEST IP: %s, DEST PORT: %d\n", inet_ntoa (req.dest_ip), req.dest_port);
	fprintf (stderr, "SRC IP: %s, SRC PORT: %d\n", inet_ntoa (src->sin_addr), src->sin_port);

	if (req.cd == 1)
		fputs ("SOCKS_CONNECT ", stderr);
	else
		fputs ("SOCKS_BIND ", stderr);

	if (rep.cd == 90) {
		fputs ("GRANTED\n", stderr);
	} else {
		fputs ("REJECTED\n", stderr);
	}

	fputs ("\n", stderr);
}

void send_reply (void)
{
	rep.dest_port = htons (rep.dest_port);
	write (STDOUT_FILENO, &rep, sizeof (Reply));
	rep.dest_port = ntohs (rep.dest_port);
}

int check_fw (void)
{
	int		i;
	char		*part;
	char		buf[MAX_BUF_SIZE];
	uint32_t	mask = 0, care = 0;	/* Network byte order */
	FILE		*conf = fopen (config_file, "r");

	/* check if the config_file exists */
	if (conf == NULL) {
		fprintf (stderr, "error: '%s' not found\n", config_file);
		return -1;
	}

	/* read rules from the config_file */
	fgets (buf, MAX_BUF_SIZE, conf);
	strtok (buf, "\r\n");
	for (i = 0; i < 4; ++i) {
		if (i == 0)
			part = strtok (buf, ".");
		else
			part = strtok (NULL, ".");
		if (isnumber (part)) {
			care |= atoi (part) << (i * 8);
		} else if (strcmp (part, "*") == 0) {
			mask |= 0xff << (i * 8);
		} else {
			/* things go wring */
			fprintf (stderr, "error: syntax error in '%s'\n", config_file);
			return -1;
		}
	}

	return !((req.dest_ip.s_addr & ~mask) ^ care);
}

int recv_req (void)
{
	char		buf[MAX_BUF_SIZE];
	struct hostent	*phe;

	read (STDIN_FILENO, buf, MAX_BUF_SIZE);
	memcpy (&req, buf, 8);

	/* check request validity */
	if (req.vn != 4) {
		fputs ("error: the server only supports version 4 for now\n", stderr);
		return -1;
	}
	if (req.cd != 1 && req.cd != 2) {
		fputs ("error: \n", stderr);
		return -1;
	}

	req.dest_port = ntohs (req.dest_port);
	strncpy (req.user, buf + 8, MAX_USER_LEN);
	strncpy (req.dn, buf + 8 + strlen (req.user) + 1, MAX_DN_LEN);
	if ((req.dest_ip.s_addr & 0xffffff) == 0) {
		if ((phe = gethostbyname (req.dn)))
			req.dest_ip = *((struct in_addr *) phe->h_addr_list[0]);
		else if ((req.dest_ip.s_addr = inet_addr (req.dn)) == INADDR_NONE) {
			fprintf (stderr, "error: cannot resolve domain name '%s'\n", req.dn);
			return -1;
		}
	}

	return 0;
}

int isnumber (char *s)
{
	int	i;
	for (i = 0; s[i] != 0; ++i)
		if (!isdigit(s[i]))
			return 0;
	return i;
}
