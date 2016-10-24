#include <stdlib.h>
#define __USE_POSIX
#include <stdio.h>
#undef __USE_POSIX
#include <stdint.h>
#include <string.h>
#include <strings.h>
#define __USE_MISC
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#undef __USE_MISC
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>

char gSc = 0;

int recv_msg (int from);
int readline (int fd, char *ptr, int maxlen);

int main (int argc, char **argv)
{
	fd_set	readfds;
	int	client_fd, len, end, SERVER_PORT;
	char	msg_buf[30000];
	FILE	*fp; 
	struct	sockaddr_in client_sin;
	struct	hostent *he; 

	gSc = 0;

	/* parse the command line arguments */
	if (argc == 2) {	/* read from stdin */
		fp = stdin;
	} else if (argc == 3) {	/* read from file */
		fp = fopen(argv[2], "r");
		if (fp == NULL) {
			fprintf(stderr,"Error : '%s' doesn't exist\n", argv[3]);
			exit(1);
		}
	} else {
		fprintf(stderr,"Usage : client <server ip> [<testfile>]\n");
		exit(1);
	}    
	
	/* resolve hostname */
	if ( (he = gethostbyname (argv[1])) == NULL ) {
		fprintf(stderr,"Usage : client <server ip> [<testfile>]\n");
		exit(1);
	}
	                             
	SERVER_PORT = 9527;
	
	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&client_sin, sizeof(client_sin));
	client_sin.sin_family = AF_INET;
	client_sin.sin_addr = *((struct in_addr *)he->h_addr); 
	client_sin.sin_port = htons(SERVER_PORT);
	if (connect(client_fd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == -1) {
		fputs ("error: connect failed\n", stderr);
		exit(1);
	}
	
	sleep(1);
	
	end = 0;

	while (1) { 
		FD_ZERO(&readfds);
		FD_SET(client_fd, &readfds);

		if (end == 0)
			FD_SET(fileno(fp), &readfds);

		if (select(client_fd + 1, &readfds, NULL, NULL, NULL) < 0)
			exit(1);

		if (gSc == 1) {
			/* send meesage */
			len = readline (fileno(fp), msg_buf, sizeof(msg_buf));
			if (len < 0) exit(1);
			msg_buf[len] = '\0';
			write (STDOUT_FILENO, msg_buf, len);	/* echo to stdout first */
			if (write (client_fd, msg_buf, len) == -1)
				return -1;
			usleep (1000);
			gSc = 0;
		}

		if (FD_ISSET(client_fd, &readfds)) {
			int errnum;
			errnum = recv_msg(client_fd);
			if (errnum < 0) {
				shutdown (client_fd, 2);
				close (client_fd);
				exit (1);
			} else if (errnum == 0){
				shutdown (client_fd, 2);
				close (client_fd);
				exit (0);
			}
		}
	}
}

int recv_msg (int from)
{
	int	len;
	char	buf[3000];
	if ( (len = readline (from, buf, sizeof(buf) - 1)) < 0 )
		return -1;
	buf[len] = 0; 
	write (STDOUT_FILENO, buf, len);
	return len;
}

int readline (int fd, char *ptr, int maxlen)
{
	int	n, rc;
	char	c;
	*ptr = 0;
	for (n = 1; n < maxlen; n++) {
		if ((rc = read(fd, &c, 1)) == 1) {
			*ptr++ = c;	
			if (c == ' ' && *(ptr - 2) == '%') {
				gSc = 1;
				break;
			} else if (c == '\n')
				break;
		} else if (rc == 0) {
			if (n == 1)
				return 0;
			else
				break;
		} else
			return -1;
	}
	return n;
}      
