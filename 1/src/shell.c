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
#define MAX_PIPE	1000


const char motd[] =	"****************************************\n"
			"** Welcome to the information server. **\n"
			"****************************************\n\n";
const char prompt[] = "% ";

typedef struct numbered_pipe {
	int	*fd;
} Npipe;

void save_fds (int *stdfd);
void restore_fds (int *stdfd);
int readline (char *line, int *connection);
int arespace (char *s);

void np_countdown (Npipe np[MAX_PIPE]);
void set_np_in (Npipe np[MAX_PIPE]);
void set_np_out (char *line, Npipe np[MAX_PIPE], int *connection);
int isnumber (char *s);
void close_np (Npipe np[MAX_PIPE]);
void clear_nps (Npipe np[MAX_PIPE]);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

void execute_one_line (int progc, char **cmds, int *connection);
int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);

void printenv (int argc, char **argv);
void setupenv (int argc, char **argv);
void open_files (const char *in_file, const char *out_file);
void set_pipes_out (int *pipefd, int *stdfd, int index, int progc);
void set_pipes_in (int *pipefd, int *stdfd, int index, int progc);


int shell (void)
{
	int	connection = 1, progc, stdfd[3], readc;
	char	line[MAX_LINE_SIZE + 1], *cmds[(MAX_LINE_SIZE - 1) / 2];
	Npipe	np[MAX_PIPE] = {0};

	/* initialize the environment variables */
	clearenv ();
	putenv ("PATH=bin:.");

	/* print the welcome message */
	write (STDOUT_FILENO, motd, strlen(motd));

	while (connection > 0) {
		/* show the prompt */
		write (STDOUT_FILENO, prompt, strlen(prompt));
		/* read one line from client input */
		if ((readc = readline (line, &connection)) < 0) {
			/* close numbered pipe for illegal input */
			close_np (np);
		} else if (readc > 0) {
			/* save original fds */
			save_fds (stdfd);
			/* add new numbered pipe and set up to output */
			set_np_out (line, np, &connection);
			/* set up the input from numbered pipe */
			set_np_in (np);
			/* parse the total input line into commands seperated by pipes */
			progc = line_to_cmds (line, cmds);
			/* execute one line command */
			execute_one_line (progc, cmds, &connection);
			/* restore original fds */
			restore_fds (stdfd);
			/* free the allocated space of commands */
			clear_cmds (progc, cmds);
			/* shift all the numbered pipes by one (count down timer) */
			np_countdown (np);
		}
	}

	/* free the allocated space of numbered pipes */
	clear_nps (np);

	return connection;
}

int readline (char *line, int *connection)
{
	int	i;
	char	*p = NULL;
	p = fgets (line, MAX_LINE_SIZE + 1, stdin);
	if (ferror (stdin)) {		/* error */
		clearerr (stdin);
		fputs ("read error: fgets failed\n", stderr);
		return -1;
	} else if (feof (stdin)) {	/* EOF */
		clearerr (stdin);
		*connection = 0;
	}
	if (p == NULL)		/* no data was read */
		return 0;
	/* check for illegal input and remove crlf */
	for (i = 0; line[i] != 0; ++i) {
		if (line[i] == '/') {
			fputs ("input error: character '/' is illegal\n", stderr);
			return -1;
		} else if (line[i] == '\r' || line[i] == '\n') {
			line[i] = 0;
			if (arespace (line))
				return 0;
			break;
		}
	}
	return strlen (line);
}

void clear_nps (Npipe np[MAX_PIPE])
{
	int	 i;
	for (i = 0; i < MAX_PIPE; ++i) {
		if (np[i].fd) {
			close (np[i].fd[0]);
			close (np[i].fd[1]);
			free (np[i].fd);
			np[i].fd = NULL;
		}
	}
}

void close_np (Npipe np[MAX_PIPE])
{
	if (np[0].fd) {
		close (np[0].fd[1]);
		close (np[0].fd[0]);
		free (np[0].fd);
		np[0].fd = NULL;
	}
}

void np_countdown (Npipe np[MAX_PIPE])
{
	int	i;
	for (i = 0; i < MAX_PIPE - 1; ++i) {
		np[i] = np[i + 1];
		np[i + 1].fd = NULL;
	}
}

void set_np_in (Npipe np[MAX_PIPE])
{
	if (np[0].fd) {
		close (np[0].fd[1]);			/* close the write end of the pipe */
		dup2 (np[0].fd[0], STDIN_FILENO);	/* duplicate the read end of the pipe to stdin */
		close (np[0].fd[0]);			/* then close the read end of the pipe */
		free (np[0].fd);			/* free the allocated space for fds */
		np[0].fd = NULL;			/* reset to NULL */
	}
}

void set_np_out (char *line, Npipe np[MAX_PIPE], int *connection)
{
	int	i, number;

	/* find the numbered pipe in the current input line and add it in */
	for (i = 0; line[i] != 0; ++i) {
		if ((line[i] == '|' || line[i] == '!') && isnumber (line + i + 1)) {
			/* the pipe number */
			number = atoi (line + i + 1);

			/* create a new pipe if it has not been created */
			if (np[number].fd == NULL) {
				np[number].fd = (int *) malloc (2 * sizeof(int));
				if (pipe (np[number].fd) < 0) {
					fputs ("server error: failed to create numbered pipes\n", stderr);
					*connection = 0;
				}
			}

			/* set up the output to pipe */
			dup2 (np[number].fd[1], STDOUT_FILENO);
			if (line[i] == '!')
				dup2 (np[number].fd[1], STDERR_FILENO);

			line[i] = 0;
			break;
		}
	}
}

void execute_one_line (int progc, char **cmds, int *connection)
{
	pid_t	childpid;
	int	i, argc, pipefd[2], stdfd[3], stat;
	char	*argv[MAX_CMD_SIZE / 2 + 1] = {0};
	char	*in_file = NULL, *out_file = NULL;

	/* save original fds, may the duplicated by numbered pipes */
	save_fds (stdfd);

	for (i = 0; i < progc; ++i) {
		/* parse the input command into argv */
		argc = cmd_to_argv (cmds[i], argv, &in_file, &out_file);

		/* execute the command accordingly */
		if (strcmp (argv[0], "exit") == 0) {
			*connection = 0;
		} else if (strcmp (argv[0], "printenv") == 0) {
			printenv (argc, argv);
		} else if (strcmp (argv[0], "setenv") == 0) {
			setupenv (argc, argv);
		} else {
			/* create a pipe */
			if (pipe (pipefd) < 0) {
				fputs ("server error: failed to create pipes\n", stderr);
				*connection = 0;
				return;
			}
			/* fork a child and exec the program */
			if ((childpid = fork()) < 0) {
				close (pipefd[0]); close (pipefd[1]);
				fputs ("server error: fork failed\n", stderr);
			} else if (childpid == 0) {
				/* set up pipe to write to */
				set_pipes_out (pipefd, stdfd, i, progc);
				open_files (in_file, out_file);
				/* invoke the command */
				execvpe (*argv, argv, environ);
				/* error handling */
				fprintf (stderr, "Unknown command: [%s].\n", *argv);
				*connection = -1;
				i = progc;
			}
			set_pipes_in (pipefd, stdfd, i, progc);
			wait (&stat);
			if (stat != 0)
				i = progc;
		}

		/* free the allocated space of one command */
		clear_argv (argc, argv, &in_file, &out_file);
	}

	/* restore original fds */
	restore_fds (stdfd);
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

void save_fds (int *stdfd)
{
	stdfd[0] = dup (STDIN_FILENO);
	stdfd[1] = dup (STDOUT_FILENO);
	stdfd[2] = dup (STDERR_FILENO);
}

void restore_fds (int *stdfd)
{
	dup2 (stdfd[0], STDIN_FILENO);
	close (stdfd[0]);
	dup2 (stdfd[1], STDOUT_FILENO);
	close (stdfd[1]);
	dup2 (stdfd[2], STDERR_FILENO);
	close (stdfd[2]);
}

void open_files (const char *in_file, const char *out_file)
{
	int	infd, outfd;
	if (in_file) {
		infd = open (in_file, O_RDONLY, 0);
		if (infd < 0) {
			fprintf (stderr, "server error: cannot open file %s\n", in_file);
			exit (1);
		}
		dup2 (infd, STDIN_FILENO);
		close (infd);
	}
	if (out_file) {
		outfd = open (out_file, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (outfd < 0) {
			fprintf (stderr, "server error: cannot open file %s\n", out_file);
			exit (1);
		}
		dup2 (outfd, STDOUT_FILENO);
		close (outfd);
	}
}

void printenv (int argc, char **argv)
{
	int	i;
	if (argc == 1) {
		for (i = 0; environ[i] != NULL; ++i) {
			char *out;
			out = (char *) malloc (strlen(environ[i]) + 2);
			sprintf (out, "%s\n", environ[i]);
			write (STDOUT_FILENO, out, strlen(out));
			free (out);
		}
	} else {
		for (i = 1; i < argc; ++i) {
			char *value = getenv (argv[i]), *out;
			if (value == NULL) {
				fprintf (stderr, "\"%s\" does not exist\n", argv[i]);
			} else {
				out = (char *) malloc (strlen(argv[i]) + 1 + strlen(value) + 2);
				sprintf (out, "%s=%s\n", argv[i], value);
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

	arg = strtok (cmd, " \r\n\t");
	while (arg != NULL) {
		if (strcmp (arg, ">") == 0) {
			arg = strtok (NULL, " \r\n\t");
			*out_file = (char *) malloc (strlen(arg) + 1);
			strcpy (*out_file, arg);
		} else if (strcmp (arg, "<") == 0) {
			arg = strtok (NULL, " \r\n\t");
			*in_file = (char *) malloc (strlen(arg) + 1);
			strcpy (*in_file, arg);
		} else {
			argv[argc] = (char *) malloc (strlen(arg) + 1);
			strcpy (argv[argc], arg);
			++argc;
		}
		arg = strtok (NULL, " \r\n\t");
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
		cmds[progc] = (char *) malloc (strlen (cmd) + 1);
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

int isnumber (char *s)
{
	int	i;
	for (i = 0; s[i] != 0; ++i)
		if (!isdigit(s[i]))
			return 0;
	return i;
}

int arespace (char *s)
{
	int	i;
	for (i = 0; s[i] != 0; ++i)
		if (!isspace(s[i]))
			return 0;
	return 1;	/* empty strings are treated as spaces */
}
