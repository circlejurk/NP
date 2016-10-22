#include <stdio.h>
#define __USE_MISC
#define __USE_XOPEN2K
#include <stdlib.h>
#undef __USE_MISC
#undef __USE_XOPEN2K
#include <ctype.h>
#include <string.h>
#define __USE_GNU
#include <unistd.h>
#undef __USE_GNU
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_LINE_SIZE	15000
#define MAX_CMD_SIZE	256


const char motd[] = "****************************************\n"
		    "** Welcome to the information server. **\n"
		    "****************************************\n\n";
const char prompt[] = "% ";

void save_stdfds (int *stdfd);
int readline (char *line, int *connection);
int check_illegal (const char *line);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

void execute_one_line (int progc, char **cmds, int *stdfd, int *connection);
void create_pipe (int *pipefd);
int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);

void printenv (int argc, char **argv);
void setupenv (int argc, char **argv);
void open_files (const char *in_file, const char *out_file);
void set_pipes_out (int *pipefd, int *stdfd, int index, int progc);
void set_pipes_in (int *pipefd, int *stdfd, int index, int progc);


int shell (void)
{
	int	connection = 1, progc, stdfd[2];
	char	line[MAX_LINE_SIZE + 1], *cmds[(MAX_LINE_SIZE - 1) / 4];

	/* initialize the environment variables */
	clearenv ();
	putenv ("PATH=bin:.");

	/* save original stdin, stdout */
	save_stdfds (stdfd);

	/* print the welcome message */
	write (STDOUT_FILENO, motd, sizeof(motd));

	while (connection) {
		/* show the prompt */
		write (STDOUT_FILENO, prompt, sizeof(prompt));

		/* read one line from client input */
		if (readline (line, &connection) == 0) {

			/* parse the total input line into commands seperated by pipes */
			progc = line_to_cmds (line, cmds);

			/* execute one line command */
			execute_one_line (progc, cmds, stdfd, &connection);

			/* free the allocated space of commands */
			clear_cmds (progc, cmds);
		}
	}

	return 0;
}

void execute_one_line (int progc, char **cmds, int *stdfd, int *connection)
{
	pid_t	childpid;
	int	i, argc, pipefd[2];
	char	*argv[MAX_CMD_SIZE / 2 + 1] = {0}, *in_file = NULL, *out_file = NULL;
	for (i = 0; i < progc; ++i) {
		/* create a pipe */
		create_pipe (pipefd);

		/* parse the input command into argv */
		argc = cmd_to_argv (cmds[i], argv, &in_file, &out_file);

		/* execute the command accordingly */
		if (*argv != NULL && strcmp (*argv, "exit") == 0) {
			*connection = 0;
		} else if (*argv != NULL && strcmp (*argv, "printenv") == 0) {
			printenv (argc, argv);
		} else if (*argv != NULL && strcmp (*argv, "setenv") == 0) {
			setupenv (argc, argv);
		} else if ((childpid = fork()) < 0) {
			fputs ("server error: fork failed\n", stderr);
			exit (1);
		} else if (childpid == 0) {
			set_pipes_out (pipefd, stdfd, i, progc);
			open_files (in_file, out_file);
			execvpe (*argv, argv, environ);
			fprintf (stderr, "Unknown command: [%s]\n", *argv);
			exit (1);
		} else {
			set_pipes_in (pipefd, stdfd, i, progc);
			wait (NULL);
		}

		/* free the allocated space of one command */
		clear_argv (argc, argv, &in_file, &out_file);
	}
}

void setupenv (int argc, char **argv)
{
	if (argc == 3) {
		if (setenv (argv[1], argv[2], 1) < 0)
			fputs ("server error: setenv() failed\n", stderr);
	} else
		fputs ("Usage: setenv <name> <value>\n", stderr);
}

void set_pipes_out (int *pipefd, int *stdfd, int index, int progc)
{
	if (index != progc - 1) {
		close (STDOUT_FILENO);
		dup (pipefd[1]);
	} else {
		close (STDOUT_FILENO);
		dup (stdfd[1]);
	}
	close (pipefd[0]);
	close (pipefd[1]);
}

void set_pipes_in (int *pipefd, int *stdfd, int index, int progc)
{
	if (index != progc - 1) {
		close (STDIN_FILENO);
		dup (pipefd[0]);
	} else {
		close (STDIN_FILENO);
		dup (stdfd[0]);
	}
	close (pipefd[0]);
	close (pipefd[1]);
}

void save_stdfds (int *stdfd)
{
	stdfd[0] = dup (STDIN_FILENO);
	stdfd[1] = dup (STDOUT_FILENO);
}

void create_pipe (int *pipefd)
{
	if (pipe(pipefd) < 0) {
		fputs ("server error: failed to create pipes\n", stderr);
		exit (1);
	}
}

void open_files (const char *in_file, const char *out_file)
{
	int infd, outfd;
	if (in_file) {
		infd = open (in_file, O_RDONLY, 0);
		close (STDIN_FILENO);
		dup (infd);
		close (infd);
	}
	if (out_file) {
		outfd = open (out_file, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		close (STDOUT_FILENO);
		dup (outfd);
		close (outfd);
	}
}

int readline (char *line, int *connection)
{
	char *p;
	p = fgets (line, MAX_LINE_SIZE + 1, stdin);
	if (ferror (stdin)) {
		fputs ("server error: read failed\n", stderr);
		exit (1);
	} else if (feof (stdin) && p == (char *)0) {	/* EOF, no data was read */
		exit (0);
	} else if (feof (stdin)) {			/* EOF, some data was read */
		*connection = 0;
	}
	return check_illegal (line);	/* check for illegal input */
}


int check_illegal (const char *line)
{
	int i = 0;
	while (line[i] != 0) {
		if (line[i] == '/') {
			fputs ("input error: character '/' is not permitted\n", stderr);
			return -1;
		} else if ( line[i] == '|' && ! isdigit (line[i+1]) && ! isspace (line[i+1]) ) {
			fprintf (stderr, "input error: \"%c%c\" cannot be recognized\n", line[i], line[i+1]);
			return -1;
		}
		++i;
	}
	return 0;
}

void printenv (int argc, char **argv)
{
	int i;
	if (argc == 1) {
		for (i = 0; environ[i] != NULL; ++i) {
			char *out;
			out = malloc (strlen(environ[i]) + 2);
			strcpy (out, environ[i]);
			strcpy (out + strlen(environ[i]), "\n");
			write (STDOUT_FILENO, out, strlen(out));
			free (out);
		}
	} else {
		for (i = 1; i < argc; ++i) {
			char *value = getenv (argv[i]), *out;
			if (value == NULL) {
				fprintf (stderr, "\"%s\" does not exist\n", argv[i]);
			} else {
				out = malloc (strlen(argv[i]) + 1 + strlen(value) + 2);
				strcpy (out, argv[i]);
				strcpy (out + strlen(argv[i]), "=");
				strcpy (out + strlen(argv[i]) + 1, value);
				strcpy (out + strlen(argv[i]) + 1 + strlen(value), "\n");
				write (STDOUT_FILENO, out, strlen(out));
				free (out);
			}
		}
	}
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
			strcpy (*out_file, arg);
		} else if (strcmp (arg, "<") == 0) {
			arg = strtok (NULL, " \r\n");
			*in_file = malloc (strlen(arg) + 1);
			strcpy (*in_file, arg);
		} else {
			argv[argc] = malloc (strlen(arg) + 1);
			strcpy (argv[argc], arg);
			++argc;
		}
		arg = strtok (NULL, " \r\n");
	}

	return argc;
}

void clear_argv (int argc, char **argv, char **in_file, char **out_file)
{
	while (--argc >= 0) {
		free (argv[argc]);
		argv[argc] = NULL;
	}
	free (*in_file);
	*in_file = NULL;
	free (*out_file);
	*out_file = NULL;
}

int line_to_cmds (char *line, char **cmds)
{
	int	progc = 0;
	char	*cmd;

	cmd = strtok (line, "|\r\n");
	while (cmd != NULL ) {
		cmds[progc] = malloc (strlen(cmd) + 1);
		strcpy (cmds[progc], cmd);
		++progc;
		cmd = strtok (NULL, "|\r\n");
	}
	return progc;
}

void clear_cmds (int progc, char **cmds)
{
	while (--progc >= 0) {
		free (cmds[progc]);
		cmds[progc] = NULL;
	}
}
