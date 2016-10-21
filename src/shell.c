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

void check_illegal (const char *line);

void printenv (const char *name);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);

int shell (void)
{
	int	i, connection = 1, argc, progc, infd, outfd;
	pid_t	childpid;
	char	line[MAX_LINE_SIZE + 1], *p, *cmds[(MAX_LINE_SIZE - 1) / 4];
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

		/* parse the total input line into commands seperated by pipes */
		progc = line_to_cmds (line, cmds);

		for (i = 0; i < progc; ++i) {
			/* parse the input command */
			argc = cmd_to_argv (cmds[i], argv, &in_file, &out_file);

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

			/* free the allocated space of one command */
			clear_argv (argc, argv, &in_file, &out_file);
		}

		/* free the allocated space of commands */
		clear_cmds (progc, cmds);
	}

	return 0;
}

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

	cmd = strtok (line, "|");
	while (cmd != NULL ) {
		cmds[progc] = malloc (strlen(cmd) + 1);
		strncpy (cmds[progc], cmd, strlen(cmd) + 1);
		++progc;
		cmd = strtok (NULL, "|");
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
