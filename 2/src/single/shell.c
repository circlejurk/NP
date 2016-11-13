#include <string.h>
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
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "single.h"

const char motd[] =	"****************************************\n"
			"** Welcome to the information server. **\n"
			"****************************************\n\n";
const char prompt[] = "% ";


int shell (int sock, User *users)
{
	int	progc, stdfd[3], readc;
	char	line[MAX_LINE_SIZE + 1], *cmds[(MAX_LINE_SIZE - 1) / 2];

	if (users[sock - 4].connection > 0) {
		/* read one line from client input */
		if ((readc = readline (line, &(users[sock - 4].connection))) < 0) {
			/* close numbered pipe for illegal input */
			close_np (users[sock - 4].np);
		} else if (readc > 0) {
			/* save original fds */
			save_fds (stdfd);
			/* add new numbered pipe and set up to output */
			set_np_out (line, sock, users);
			/* set up the input from numbered pipe */
			set_np_in (users[sock - 4].np);
			/* parse the total input line into commands seperated by pipes */
			progc = line_to_cmds (line, cmds);
			/* execute one line command */
			execute_one_line (progc, cmds, sock, users);
			/* restore original fds */
			restore_fds (stdfd);
			/* free the allocated space of commands */
			clear_cmds (progc, cmds);
			/* shift all the numbered pipes by one (count down timer) */
			np_countdown (users[sock - 4].np);
		}
	}

	return users[sock - 4].connection;
}

int readline (char *line, int *connection)
{
	int	i;
	char	*p = NULL;
	p = fgets (line, MAX_LINE_SIZE + 1, stdin);
	if (ferror (stdin)) {
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
			line[i + 1] = 0;
			if (arespace (line))
				return 0;
			break;
		}
	}
	return strlen (line);
}

void clear_nps (int sock, User *users)
{
	int	i;
	if (users[sock - 4].np) {
		for (i = 0; i < MAX_PIPE; ++i) {
			if (users[sock - 4].np[i].fd) {
				close (users[sock - 4].np[i].fd[0]);
				close (users[sock - 4].np[i].fd[1]);
				free (users[sock - 4].np[i].fd);
				users[sock - 4].np[i].fd = NULL;
			}
		}
		free (users[sock - 4].np);
		users[sock - 4].np = NULL;
	}
}

void close_np (Npipe *np)
{
	if (np && np[0].fd) {
		close (np[0].fd[1]);
		close (np[0].fd[0]);
		free (np[0].fd);
		np[0].fd = NULL;
	}
}

void np_countdown (Npipe *np)
{
	if (np) {
		int	i;
		for (i = 0; i < MAX_PIPE - 1; ++i) {
			np[i] = np[i + 1];
			np[i + 1].fd = NULL;
		}
	}
}

void set_np_in (Npipe *np)
{
	if (np && np[0].fd) {
		close (np[0].fd[1]);
		dup2 (np[0].fd[0], STDIN_FILENO);
		close (np[0].fd[0]);
		free (np[0].fd);
		np[0].fd = NULL;
	}
}

void set_np_out (char *line, int sock, User *users)
{
	int	i, number;

	/* find the numbered pipe in the current input line and add it in */
	for (i = 0; line[i] != 0; ++i) {
		if ((line[i] == '|' || line[i] == '!') && isnumber (line + i + 1)) {
			/* construct the numbered pipe */
			if (users[sock - 4].np == NULL)
				users[sock - 4].np = calloc (MAX_PIPE, sizeof (Npipe));

			/* the pipe number */
			number = atoi (line + i + 1);

			/* create a new pipe if it has not been created */
			if (users[sock - 4].np[number].fd == NULL) {
				users[sock - 4].np[number].fd = (int *) malloc (2 * sizeof(int));
				if (pipe (users[sock - 4].np[number].fd) < 0) {
					fputs ("server error: failed to create numbered pipes\n", stderr);
					users[sock - 4].connection = 0;
				}
			}

			/* set up the output to pipe */
			close (STDOUT_FILENO);
			dup (users[sock - 4].np[number].fd[1]);
			if (line[i] == '!') {
				close (STDERR_FILENO);
				dup (users[sock - 4].np[number].fd[1]);
			}

			line[i] = 0;
			break;
		}
	}
}

void execute_one_line (int progc, char **cmds, int sock, User *users)
{
	pid_t	childpid;
	int	i, argc, pipefd[2], stdfd[3], stat, userpipe[2], to, from;
	char	*argv[MAX_CMD_SIZE / 2 + 1] = {0}, *in_file = NULL, *out_file = NULL, ori_cmd[MAX_CMD_SIZE + 1] = {0};

	/* save original fds */
	save_fds (stdfd);

	for (i = 0; i < progc; ++i) {
		/* save the original command */
		strncpy (ori_cmd, cmds[i], MAX_CMD_SIZE + 1);
		/* resolve user pipes from commmand */
		if (resolv_ups (cmds[i], userpipe, &to, &from, sock, users) < 0)
			break;
		set_up_from (sock, users, &from, userpipe[0], ori_cmd);
		/* parse the input command into argv */
		argc = cmd_to_argv (cmds[i], argv, &in_file, &out_file);
		/* execute the command accordingly */
		if (*argv != NULL && strcmp (*argv, "exit") == 0) {
			users[sock - 4].connection = 0;
		} else if (*argv != NULL && strcmp (*argv, "printenv") == 0) {
			printenv (argc, argv);
		} else if (*argv != NULL && strcmp (*argv, "setenv") == 0) {
			setupenv (argc, argv);
		} else if (*argv != NULL && strcmp (*argv, "who") == 0) {
			who (sock, users);
		} else if (*argv != NULL && strcmp (*argv, "name") == 0) {
			name (sock, users, argv[1]);
		} else if (*argv != NULL && strcmp (*argv, "tell") == 0) {
			tell (sock, users, argc, argv);
		} else if (*argv != NULL && strcmp (*argv, "yell") == 0) {
			yell (sock, users, argc, argv);
		} else {
			/* create a pipe */
			if (pipe (pipefd) < 0) {
				fputs ("server error: failed to create pipes\n", stderr);
				users[sock - 4].connection = 0;
				return;
			}
			/* fork a child and exec the program */
			if ((childpid = fork()) < 0) {
				close (pipefd[0]); close (pipefd[1]);
				fputs ("server error: fork failed\n", stderr);
			} else if (childpid == 0) {
				set_pipes_out (pipefd, stdfd, i, progc);
				open_files (in_file, out_file);
				set_up_to (sock, users, &to, userpipe[1], ori_cmd);
				execvpe (*argv, argv, environ);
				fprintf (stderr, "Unknown command: [%s].\n", *argv);
				users[sock - 4].connection = -1;
				i = progc;
			}
			if (to)
				close (userpipe[1]);
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

void set_up_to (int sock, User *users, int *to, int up_w, char *ori_cmd)
{
	char	msg[MAX_MSG_SIZE + 1] = {0};
	/* set up user pipes to pipe to others */
	if (*to) {
		dup2 (up_w, STDOUT_FILENO);
		close (up_w);
		snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", users[sock - 4].name, sock - 3, ori_cmd, users[*to - 1].name, *to);
		broadcast (msg, sock, users);
		*to = 0;
	}
}

void set_up_from (int sock, User *users, int *from, int up_r, char *ori_cmd)
{
	char	msg[MAX_MSG_SIZE + 1] = {0};
	/* set up user pipes to receive from others */
	if (*from) {
		dup2 (up_r, STDIN_FILENO);
		close (up_r);
		snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", users[sock - 4].name, sock - 3, users[*from - 1].name, *from, ori_cmd);
		broadcast (msg, sock, users);
		*from = 0;
	}
}

int resolv_ups (char *cmd, int userpipe[2], int *to, int *from, int sock, User *users)
{
	int	i, j, pipefd[2];
	char	msg[MAX_MSG_SIZE + 1], remain[MAX_CMD_SIZE + 1];
	userpipe[0] = userpipe[1] = *to = *from = 0;
	/* resolve the input command and remove the user pipes' part */
	for (i = 0; cmd[i] != 0; ++i) {
		if ((cmd[i] == '>' || cmd[i] == '<') && cmd[i + 1] != ' ') {
			for (j = i + 1; isdigit (cmd[j]); ++j);
			if (cmd[j] == 0 || isspace (cmd[j])) {
				strncpy (remain, cmd + j, MAX_CMD_SIZE + 1);
				cmd[j] = 0;
				if (cmd[i] == '>') {
					if ((*to = atoi (cmd + i + 1)) > 0) {
						if (users[*to - 1].connection == 0) {
							snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the user #%d does not exist. ***\n", *to);
							write (STDERR_FILENO, msg, strlen (msg));
							*to = 0;
							return -1;
						} else if (users[*to - 1].up[sock - 4] != 0) {
							snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the pipe #%d->#%d already exists. ***\n", sock - 3, *to);
							write (STDERR_FILENO, msg, strlen (msg));
							*to = 0;
							return -1;
						} else if (pipe (pipefd) < 0) {
							snprintf (msg, MAX_MSG_SIZE + 1, "error: failed to create user pipes\n");
							write (STDERR_FILENO, msg, strlen (msg));
							*to = 0;
							return -1;
						} else {
							users[*to - 1].up[sock - 4] = pipefd[0];
							userpipe[1] = pipefd[1];
						}
					}
				} else {
					if ((*from = atoi (cmd + i + 1)) > 0) {
						if (users[*from - 1].connection == 0) {
							snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the user #%d does not exist. ***\n", *from);
							write (STDERR_FILENO, msg, strlen (msg));
							*from = 0;
							return -1;
						} else if (users[sock - 4].up[*from - 1] == 0) {
							snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", *from, sock - 3);
							write (STDERR_FILENO, msg, strlen (msg));
							*from = 0;
							return -1;
						} else {
							userpipe[0] = users[sock - 4].up[*from - 1];
							users[sock - 4].up[*from - 1] = 0;
						}
					}
				}
				cmd[i] = ' ';
				cmd[i + 1] = 0;
				strcat (cmd, remain);
			}
		}
	}
	return 0;
}

void clear_ups (int sock, User *users)
{
	int	i;
	for (i = 0; i < MAX_USERS; ++i) {
		if (users[sock - 4].up[i])
			close (users[sock - 4].up[i]);
	}
	free (users[sock - 4].up);
	users[sock - 4].up = NULL;
}

void yell (int sock, User *users, int argc, char **argv)
{
	int	i;
	char	*msg = malloc (strlen (users[sock - 4].name) + 17 + MAX_MSG_SIZE + 1);
	sprintf (msg, "*** %s yelled ***: ", users[sock - 4].name);
	for (i = 1; i < argc; ++i) {
		strcat (msg, argv[i]);
		if (i == argc - 1)
			strcat (msg, "\n");
		else
			strcat (msg, " ");
	}
	broadcast (msg, sock, users);
	free (msg);
}

void tell (int sock, User *users, int argc, char **argv)
{
	int	i, to_sock = atoi (argv[1]) + 3;
	char	*msg = malloc (strlen (users[sock - 4].name) + 19 + MAX_MSG_SIZE + 1);
	sprintf (msg, "*** %s told you ***: ", users[sock - 4].name);
	for (i = 2; i < argc; ++i) {
		strcat (msg, argv[i]);
		if (i == argc - 1)
			strcat (msg, "\n% ");
		else
			strcat (msg, " ");
	}
	write (to_sock, msg, strlen (msg));
	free (msg);
}

void name (int sock, User *users, char *new_name)
{
	char	msg[MAX_MSG_SIZE + 1];
	strncpy (users[sock - 4].name, new_name, NAME_SIZE);
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s/%d is named '%s'. ***\n", users[sock - 4].ip, users[sock - 4].port, users[sock - 4].name);
	broadcast (msg, sock, users);
}

void who (int sock, User *users)
{
	int	idx;
	char	msg[MAX_MSG_SIZE + 1];
	for (idx = 0; idx < MAX_USERS; ++idx) {
		if (users[idx].connection > 0) {
			if (strlen (users[idx].name) < 8)
				snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t\t%s/%d", idx + 1, users[idx].name, users[idx].ip, users[idx].port);
			else
				snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d", idx + 1, users[idx].name, users[idx].ip, users[idx].port);
			if (idx + 4 == sock)
				strcat (msg, "\t\t<- me\n");
			else
				strcat (msg, "\n");
			write (STDOUT_FILENO, msg, strlen(msg));
		}
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
		close (STDIN_FILENO);
		dup (infd);
		close (infd);
	}
	if (out_file) {
		outfd = open (out_file, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (outfd < 0) {
			fprintf (stderr, "server error: cannot open file %s\n", out_file);
			exit (1);
		}
		close (STDOUT_FILENO);
		dup (outfd);
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

	arg = strtok (cmd, " \r\n");
	while (arg != NULL) {
		if (strcmp (arg, ">") == 0) {
			arg = strtok (NULL, " \r\n");
			*out_file = (char *) malloc (strlen(arg) + 1);
			strcpy (*out_file, arg);
		} else if (strcmp (arg, "<") == 0) {
			arg = strtok (NULL, " \r\n");
			*in_file = (char *) malloc (strlen(arg) + 1);
			strcpy (*in_file, arg);
		} else {
			argv[argc] = (char *) malloc (strlen(arg) + 1);
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
		cmds[progc] = (char *) malloc (strlen(cmd) + 1);
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
