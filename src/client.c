#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <stdint.h>
#define __USE_MISC
#include <unistd.h>
#undef __USE_MISC
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_BUF_SIZE 30000

char usage[] = "Usage: client host port [input_file]\n";

void parse (int argc, char **argv);
void openfile (char *file);
int connectTCP (char *host, int port);

int send_cmd (int sockfd, char *buf);
int receive (int sockfd, char *buf, int *connection, int *tosend);
int readline (char *buf, int max);


int main (int argc, char **argv)
{
	fd_set	rfds;
	int	sockfd, connection = 1, tosend = 0;
	char	buf[MAX_BUF_SIZE];

	/* parse the command line arguments */
	parse (argc, argv);

	/* build a TCP connection to server */
	sockfd = connectTCP (argv[1], atoi (argv[2]));

	FD_ZERO (&rfds);
	while (connection) {
		FD_SET (sockfd, &rfds);

		if (tosend) {
			if (send_cmd (sockfd, buf) < 0)
				return -1;
			tosend = 0;
		}

		if (select (sockfd + 1, &rfds, NULL, NULL, NULL) < 0)
			exit(1);

		if (FD_ISSET (sockfd, &rfds)) {
			if (receive (sockfd, buf, &connection, &tosend) < 0) {
				fputs ("receive failed\n", stderr);
				close (sockfd);
				return -1;
			}
		}
	}

	return 0;
}

int send_cmd (int sockfd, char *buf)
{
	int len;

	if ((len = readline (buf, MAX_BUF_SIZE)) < 0) {
		fputs ("failed to read input\n", stderr);
		return -1;
	}
	fputs (buf, stdout);

	if (write (sockfd, buf, strlen(buf)) < 0) {
		fputs ("failed to write to socket\n", stderr);
		return -1;
	}

	return 0;
}

int receive (int sockfd, char *buf, int *connection, int *tosend)
{
	int savefd, len;

	/* save the original stdin */
	savefd = dup (STDIN_FILENO);
	close (STDIN_FILENO);
	dup (sockfd);

	if ((len = readline (buf, MAX_BUF_SIZE)) > 0) {
		fputs (buf, stdout);
		if (strcmp (buf, "% ") == 0) {
			*tosend = 1;
		}
	} else if (len == 0) {
		*connection = 0;
	}

	/* restore the original stdin */
	close (STDIN_FILENO);
	dup (savefd);
	close (savefd);

	return len;
}

int readline (char *buf, int max)
{
	int len, rc;
	char c = 0;
	for (len = 0; len < max && c != '\n'; ++len) {
		if ((rc = read (STDIN_FILENO, &c, 1)) < 0) {
			return -1;
		} else if (rc == 0) {	/* EOF */
			break;
		} else {
			*(buf++) = c;
			if (c == ' ' && *(buf - 2) == '%')
				c = '\n';
		}
	}
	*buf = 0;
	return len;
}

void parse (int argc, char **argv)
{
	if (argc == 4) {	/* read from file */
		openfile (argv[3]);
	} else if (argc != 3) {
		fputs (usage, stderr);
		exit (1);
	}
}

void openfile (char *file)
{
	int fd = open (file, O_RDONLY, 0);
	if (fd < 0) {
		fprintf (stderr, "server error: cannot open file %s\n", file);
		exit (1);
	}
	close (STDIN_FILENO);
	dup (fd);
	close (fd);
}

int connectTCP (char *host, int port)
{
	int			sockfd;
	struct hostent		*phe;
	struct sockaddr_in	serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("client error: cannot open socket\n", stderr);
		exit (1);
	}

	/* set up server socket addr */
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	if ((phe = gethostbyname (host)))
		bcopy (phe->h_addr_list[0], (char *)&serv_addr.sin_addr, phe->h_length);
	else if ((serv_addr.sin_addr.s_addr = inet_addr (host)) == INADDR_NONE) {
		fprintf (stderr, "cannot find host: \"%s\"\n", host);
		exit (1);
	}
	serv_addr.sin_port = htons (port);

	/* connect to server */
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("error: connect failed\n", stderr);
		exit(1);
	}

	return sockfd;
}
