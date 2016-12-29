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

Request		req;
Reply		rep;
const char config_file[] = "socks.conf";

int socks (struct sockaddr_in src)
{
	struct sockaddr_in	dest;

	/* receive the SOCKS4 request from src */
	if (recv_req () < 0) {
		fputs ("error: recv_req() failed\n", stderr);
		return -1;
	}

	/* check firewall rules and send the SOCKS4 reply accordingly */
	if (check_fw ()) {
		/* reply that the connection is granted */
		return 0;
	} else {
		/* reply that the connection is rejected */
		return -1;
	}
	/* print the verbose messages to the server side */

	/* build the connection according to the modes */
	if (req.cd == 1)	/* CONNECT mode */
		/*CONNECT ();*/
		;
	else if (req.cd == 2)	/* BIND mode */
		/*BIND ();*/
		;

	return 0;
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

	/* check request validity */
	if (req.vn != 4) {
		fputs ("error: the server only supports version 4 for now\n", stderr);
		return -1;
	}
	if (req.cd != 1 && req.cd != 2) {
		fputs ("error: \n", stderr);
		return -1;
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
