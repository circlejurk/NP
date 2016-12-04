#include <string.h>
#define __USE_ISOC99
#define __USE_POSIX
#include <stdio.h>
#undef __USE_ISOC99
#undef __USE_POSIX
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#define __USE_POSIX
#include <signal.h>
#undef __USE_POSIX
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUF_SIZE 3000

typedef struct header {
	char	*method;
	char	*path;
	char	*fullpath;
	char	*qstring;
	char	*proto;
	char	*host;
	char	*port;
	char	*content_len;
	char	*user_agent;
	char	*connection;
	char	*dnt;
} Header;

void reply (char *msg);
int arespace (char *s);
void read_req (Header *req);
void clear_req (Header *req);
void set_envs (Header *req);
void OK (char *proto);
void forbidden (char *proto);
void notfound (char *proto);
void readfile (char *file);
char DocRoot[64] = "/u/cs/103/0310004/public_html";

int httpd (void)
{
	Header	req = {0};

	read_req (&req);
	set_envs (&req);

	if (access (req.fullpath, F_OK|X_OK) != -1) {
		OK (req.proto);
		execl (req.fullpath, req.fullpath, NULL);
	} else if (access (req.fullpath, F_OK|R_OK) != -1) {
		OK (req.proto);
		readfile (req.fullpath);
	} else if (access (req.fullpath, F_OK) != -1) {
		forbidden (req.proto);
	} else {
		notfound (req.proto);
	}

	clear_req (&req);

	return 0;
}

void readfile (char *file)
{
	FILE	*fin = fopen (file, "r");
	char	buf[MAX_BUF_SIZE + 1];

	reply ("Content-Type: text/html\n\n");

	while (fgets (buf, MAX_BUF_SIZE + 1, fin))
		reply (buf);
}

void OK (char *proto)
{
	char	buf[MAX_BUF_SIZE + 1];
	snprintf (buf, MAX_BUF_SIZE + 1,
			"%s 200 OK\n"
			"Server: sake\n", proto);
	reply (buf);
}

void forbidden (char *proto)
{
	char	buf[MAX_BUF_SIZE + 1];
	snprintf (buf, MAX_BUF_SIZE + 1,
			"%s 403 Forbidden\n"
			"Server: sake\n"
			"Content-Type: text/plain\n\n"
			"403 Forbidden\n", proto);
	reply (buf);
}

void notfound (char *proto)
{
	char	buf[MAX_BUF_SIZE + 1];
	snprintf (buf, MAX_BUF_SIZE + 1,
			"%s 404 Not Found\n"
			"Server: sake\n"
			"Content-Type: text/plain\n\n"
			"404 Not Found\n", proto);
	reply (buf);
}

void set_envs (Header *req)
{
	clearenv ();
	setenv ("SERVER_SOFTWARE", "sake", 1);
	setenv ("REDIRECT_STATUS", "200", 1);
	setenv ("DOCUMENT_ROOT", DocRoot, 1);
	setenv ("REQUEST_METHOD", req->method, 1);
	setenv ("REQUEST_URI", req->path, 1);
	setenv ("SCRIPT_NAME", req->path, 1);
	setenv ("SCRIPT_FULLNAME", req->fullpath, 1);
	setenv ("QUERY_STRING", req->qstring, 1);
	setenv ("SERVER_PROTOCOL", req->proto, 1);
	if (req->host) {
		setenv ("HTTP_HOST", req->host, 1);
		setenv ("SERVER_NAME", req->host, 1);
	}
	if (req->port)
		setenv ("SERVER_PORT", req->port, 1);
	if (req->content_len)
		setenv ("CONTENT_LENGTH", req->content_len, 1);
	/* the 7 lines following will be deleted after demo */
	else
		setenv ("CONTENT_LENGTH", "", 1);
	setenv ("REMOTE_HOST", "remote-host", 1);
	setenv ("REMOTE_ADDR", "remote-addr", 1);
	setenv ("AUTH_TYPE", "auth-type", 1);
	setenv ("REMOTE_USER", "remote-user", 1);
	setenv ("REMOTE_IDENT", "remote-ident", 1);
	/* the 7 lines above will be deleted after demo */
	if (req->user_agent)
		setenv ("HTTP_USER_AGENT", req->user_agent, 1);
	if (req->connection)
		setenv ("HTTP_CONNECTION", req->connection, 1);
	if (req->dnt)
		setenv ("HTTP_DNT", req->dnt, 1);

	/* TODO: add SERVER_ADDR */
}

void clear_req (Header *req)
{
	if (req->method)
		free (req->method);
	if (req->path)
		free (req->path);
	if (req->fullpath)
		free (req->fullpath);
	if (req->qstring)
		free (req->qstring);
	if (req->proto)
		free (req->proto);
	if (req->host)
		free (req->host);
	if (req->port)
		free (req->port);
	if (req->content_len)
		free (req->content_len);
	if (req->user_agent)
		free (req->user_agent);
	if (req->connection)
		free (req->connection);
	if (req->dnt)
		free (req->dnt);
}

void read_req (Header *req)
{
	char	buf[MAX_BUF_SIZE + 1], *token, *q, *key, *val;

	fgets (buf, MAX_BUF_SIZE + 1, stdin);
	/* method */
	token = strtok (buf, " \r\n");
	req->method = malloc (strlen (token) + 1);
	strncpy (req->method, token, 5);
	/* path */
	token = strtok (NULL, " \r\n");
	for (q = token; *q != '?' && *q != 0; ++q);
	if (*q == '?') {
		*q = 0;
		++q;
	}
	req->path = malloc (strlen (token) + 1);
	strncpy (req->path, token, strlen (token) + 1);
	/* query string */
	req->qstring = malloc (strlen (q) + 1);
	strncpy (req->qstring, q, strlen (q) + 1);
	/* HTTP protocol */
	token = strtok (NULL, " \r\n");
	req->proto = malloc (strlen (token) + 1);
	strncpy (req->proto, token, strlen (token) + 1);
	/* fullpath */	/* TODO: user_dir, change DocRoot before first usage */
	req->fullpath = malloc (strlen (DocRoot) + strlen (req->path) + 1);
	strncpy (req->fullpath, DocRoot, strlen (DocRoot) + 1);
	strncat (req->fullpath, req->path, strlen (req->path) + 1);

	/* read through each line of the request header */
	while (fgets (buf, MAX_BUF_SIZE + 1, stdin)) {
		if (arespace (buf))
			break;
		strtok (buf, "\r\n");
		/* setting up the key value pair */
		key = buf;
		for (val = buf; *val != ':' && *val != 0; ++val);
		if (*val == ':') {
			*val = 0;
			++val;
		}
		while (isspace (*val))
			++val;

		/* set up the value you're interested in */
		if (strcmp (key, "Host") == 0) {
			/* seperate the hostname and port number */
			char	*hn = val, *port = val;
			for (port = val; *port != ':' && *port != 0; ++port);
			if (*port == ':') {
				*port = 0;
				++port;
				req->port = malloc (strlen (port) + 1);
				strncpy (req->port, port, strlen (port) + 1);
			} else {
				req->port = malloc (3);
				strncpy (req->port, "80", 3);
			}
			req->host = malloc (strlen (hn) + 1);
			strncpy (req->host, hn, strlen (hn) + 1);
		} else if (strcmp (key, "Content-Length") == 0) {
			req->content_len = malloc (strlen (val) + 1);
			strncpy (req->content_len, val, strlen (val) + 1);
		} else if (strcmp (key, "User-Agent") == 0) {
			req->user_agent = malloc (strlen (val) + 1);
			strncpy (req->user_agent, val, strlen (val) + 1);
		} else if (strcmp (key, "Connection") == 0) {
			req->connection = malloc (strlen (val) + 1);
			strncpy (req->connection, val, strlen (val) + 1);
		} else if (strcmp (key, "DNT") == 0) {
			req->dnt = malloc (strlen (val) + 1);
			strncpy (req->dnt, val, strlen (val) + 1);
		}
	}
}

void reply (char *msg)
{
	if (msg != NULL)
		write (STDOUT_FILENO, msg, strlen (msg));
}

int arespace (char *s)
{
	int	i;
	for (i = 0; s[i] != 0; ++i)
		if (!isspace(s[i]))
			return 0;
	return 1;	/* empty strings are treated as spaces */
}


void initialize (void);
void reaper (int sig);
int passiveTCP (int port, int qlen);
int httpd (void);
const char usage[] = "Usage: ./server [<port>]\n";

int main (int argc, char **argv)
{
	int			msock, ssock, port = 80;
	socklen_t		clilen = sizeof (struct sockaddr_in);
	pid_t			childpid;
	struct sockaddr_in	cli_addr;

	initialize ();	/* server initialization */

	/* setting up the port number */
	if (argc == 2) {
		port = atoi (argv[1]);
	} else if (argc > 2) {
		fputs (usage, stderr);
		return -1;
	}

	/* build a TCP connection */
	if ((msock = passiveTCP (port, 0)) < 0) {
		fputs ("error: passiveTCP failed\n", stderr);
		return -1;
	}

	while (1) {
		/* accept connection request */
		ssock = accept (msock, (struct sockaddr *)&cli_addr, &clilen);
		if (ssock < 0) {
			if (errno == EINTR)
				continue;
			fputs ("error: accept failed\n", stderr);
			return -1;
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			dup2 (ssock, STDIN_FILENO);
			dup2 (ssock, STDOUT_FILENO);
			/*dup2 (ssock, STDERR_FILENO);*/
			close (msock);
			close (ssock);
			exit (httpd ());
		}
		close (ssock);
	}

	return 0;
}

void initialize (void)
{
	/* initialize the original directory */
	chdir (DocRoot);
	/* establish a signal handler for SIGCHLD */
	signal (SIGCHLD, reaper);
}

void reaper (int sig)
{
	while (waitpid (-1, NULL, WNOHANG) > 0);
	signal (sig, reaper);
}

int passiveTCP (int port, int qlen)
{
	int			sockfd;
	struct sockaddr_in	serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("error: cannot open socket\n", stderr);
		return -1;
	}

	/* set up server socket addr */
	bzero (&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (port);

	/* bind to server address */
	if (bind (sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("error: cannot bind local address\n", stderr);
		return -1;
	}

	/* listen for requests */
	if (listen (sockfd, qlen) < 0) {
		fputs ("error: listen failed\n", stderr);
		return -1;
	}

	return sockfd;
}
