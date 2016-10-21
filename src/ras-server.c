#include <stdio.h>
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
#include <ctype.h>
#include <string.h>
#include <strings.h>
#define __USE_GNU
#include <unistd.h>
#undef __USE_GNU
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define SERV_TCP_PORT	9527
#define MAX_LINE_SIZE	15000
#define MAX_CMD_SIZE	256

const char motd[] = "****************************************\n"
		    "** Welcome to the information server. **\n"
		    "****************************************\n\n";
const char prompt[] = "% ";


void check_illegal (const char *line)
{
	int i = 0;
	while (line[i] != 0) {
		if (line[i] == '/') {
			fputs ("input error: character '/' is not permitted\n", stderr);
			exit (1);
		} else if ( line[i] == '|' && ! isdigit (line[i+1]) && ! isspace (line[i+1]) ) {
			fprintf (stderr, "input error: \"%c%c\" cannot be recognized\n", line[i], line[i+1]);
			exit (1);
		}
		++i;
	}
}

void printenv (const char *name)
{
	char *value = getenv (name), *out;
	if (value == NULL)
		return;
	out = malloc (strlen(name) + 1 + strlen(value) + 1);
	strncpy (out, name, strlen(name));
	strncpy (out + strlen(name), "=", 1);
	strncpy (out + strlen(name) + 1, value, strlen(value));
	strncpy (out + strlen(name) + 1 + strlen(value), "\n", 1);
	write (STDOUT_FILENO, out, strlen(name) + 1 + strlen(value) + 1);
	free (out);
}

int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file)
{
	int	argc = 0;
	char	*arg;

	arg = strtok (cmd, " \r\n");
	while (arg != NULL) {
		if (strcmp (arg, ">") == 0) {
			arg = strtok (NULL, " \r\n");
			*out_file = malloc (strlen(arg) + 1);
			strncpy (*out_file, arg, strlen(arg) + 1);
		} else if (strcmp (arg, "<") == 0) {
			arg = strtok (NULL, " \r\n");
			*in_file = malloc (strlen(arg) + 1);
			strncpy (*in_file, arg, strlen(arg) + 1);
		} else {
			argv[argc] = malloc (strlen(arg) + 1);
			strncpy (argv[argc], arg, strlen(arg) + 1);
			++argc;
		}
		arg = strtok (NULL, " \r\n");
	}

	return argc;
}

void clear_argv (int argc, char **argv, char **in_file, char **out_file)
{
	int i;
	for (i = 0; i < argc; ++i) {
		free (argv[i]);
		argv[i] = NULL;
	}
	free (*in_file);
	*in_file = NULL;
	free (*out_file);
	*out_file = NULL;
}

int shell (void)
{
	int	connection = 1, argc, infd, outfd;
	pid_t	childpid;
	char	line[MAX_LINE_SIZE + 1], *p;
	char	*argv[MAX_CMD_SIZE / 2 + 1] = {0}, *in_file = NULL, *out_file = NULL;

	/* initialize the environment variables */
	clearenv ();
	putenv ("PATH=bin:.");

	/* print the welcome message */
	write (STDOUT_FILENO, motd, sizeof(motd));

	while (connection) {
		/* show the prompt */
		write (STDOUT_FILENO, prompt, sizeof(prompt));

		/* read one line from client input */
		p = fgets (line, MAX_LINE_SIZE + 1, stdin);
		if (ferror (stdin)) {
			fputs ("server error: read failed\n", stderr);
			exit (1);
		} else if (feof (stdin) && p == (char *)0) {	/* EOF, no data was read */
			return 0;
		} else if (feof (stdin)) {			/* EOF, some data was read */
			connection = 0;
		}

		/* check for illegal input */
		check_illegal (line);

		/* parse the input command */
		argc = cmd_to_argv (line, argv, &in_file, &out_file);

		/* execute the command accordingly */
		if (*argv != NULL && strcmp (*argv, "exit") == 0) {
			connection = 0;
		} else if (*argv != NULL && strcmp (*argv, "printenv") == 0) {
			if (argc == 2)
				printenv (argv[1]);
		} else if (*argv != NULL && strcmp (*argv, "setenv") == 0) {
			if (argc == 3)
				setenv (argv[1], argv[2], 1);
		} else if ((childpid = fork()) < 0) {	/* fork a child to execute the command */
			fputs ("server error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			/* open files if redirections are used */
			if (in_file) {
				infd = open (in_file, O_RDONLY, 0);
				close (STDIN_FILENO);
				dup (infd);
				close (infd);
			}
			if (out_file) {
				/* open(create) file with mode 644 */
				outfd = open (out_file, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
				close (STDOUT_FILENO);
				dup (outfd);
				close (outfd);
			}
			/* exec the program */
			execvpe (*argv, argv, environ);
			fprintf (stderr, "Unknown command: [%s]\n", *argv);
			exit (1);
		} else {
			wait (NULL);
		}

		/* free the allocated space */
		clear_argv (argc, argv, &in_file, &out_file);
	}

	return 0;
}


int main (void)
{
	int			sockfd, newsockfd;
	socklen_t		clilen;
	pid_t			childpid;
	struct sockaddr_in	cli_addr, serv_addr;

	/* open a TCP socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("server error: cannot open socket\n", stderr);
		exit (1);
	}

	/* setup server socket addr */
	bzero (&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (SERV_TCP_PORT);

	/* bind to server address */
	if (bind (sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		fputs ("server error: cannot bind local address\n", stderr);
		exit (1);
	}

	/* listen for requests */
	if (listen (sockfd, 0) < 0) {
		fputs ("server error: listen failed\n", stderr);
		exit (1);
	}

	while (1) {
		/* accept connection request */
		clilen = sizeof(cli_addr);
		newsockfd = accept (sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (newsockfd < 0) {
			fputs ("server error: accept failed\n", stderr);
			exit (1);
		}

		/* fork another process to handle the request */
		if ((childpid = fork ()) < 0) {
			fputs ("server error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			close (STDIN_FILENO);	close (STDOUT_FILENO);	close (STDERR_FILENO);
			dup (newsockfd);	dup (newsockfd);	dup (newsockfd);
			close (sockfd);
			close (newsockfd);
			shell ();
			exit (0);
		}
		close (newsockfd);
	}

	return 0;
}
