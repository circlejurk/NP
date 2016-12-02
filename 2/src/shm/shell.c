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
#define __USE_POSIX
#include <signal.h>
#undef __USE_POSIX
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define __USE_MISC
#include <sys/shm.h>
#undef __USE_MISC
#include <errno.h>

#include "shm.h"

const char motd[] =	"****************************************\n"
			"** Welcome to the information server. **\n"
			"****************************************\n\n";
const char prompt[] = "% ";
const char base_dir[] = "/u/cs/103/0310004";

static void sig_handler (int sig);

static int	shmid = 0, uid = 0;
static User	*users = NULL;

int shell (struct sockaddr_in *addr)
{
	int	connection = 1, progc, stdfd[3], readc;
	char	line[MAX_LINE_SIZE + 1], *cmds[(MAX_LINE_SIZE - 1) / 2];
	Npipe	*np = NULL;

	initialize ();	/* server initialization */

	/* add the client into the users */
	if (add_user (addr) < 0) {
		shmdt (users);
		return -1;
	}

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
			set_np_out (line, &np, &connection);
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
	clear_nps (&np);

	/* remove the user entry from users */
	if (users[uid].pid == getpid ())
		rm_user ();

	/* detach the shared memory segment */
	shmdt (users);

	return connection;
}

void broadcast (char *msg)
{
	int	i, j;
	write (STDOUT_FILENO, msg, strlen (msg));
	strncat (msg, prompt, strlen (prompt) + 1);
	for (i = 0; i < MAX_USERS; ++i) {
		if (users[i].id > 0 && i != uid) {
			for (j = 0; j < MAX_MSG_NUM; ++j) {
				if (users[i].msg[uid][j][0] == 0) {
					strncpy (users[i].msg[uid][j], msg, MAX_MSG_SIZE + 1);
					kill (users[i].pid, SIGUSR1);
					break;
				}
			}
		}
	}
}

void rm_user (void)
{
	int	i;
	char	msg[MAX_MSG_SIZE + 1];
	/* broadcast that you're out */
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' left. ***\n", users[uid].name);
	broadcast (msg);
	/* close the fds */
	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);
	/* close fifo fds */
	for (i = 0; i < MAX_USERS; ++i) {
		if (users[uid].fifo[i].fd) {
			close (users[uid].fifo[i].fd);
			unlink (users[uid].fifo[i].name);
		}
	}
	/* clear the user entry */
	memset (&users[uid], 0, sizeof (User));
}

int add_user (struct sockaddr_in *addr)
{
	char	msg[MAX_MSG_SIZE + 1];
	for (uid = 0; uid < MAX_USERS; ++uid) {
		if (users[uid].id == 0) {
			/* initialize the user entry */
			users[uid].id = uid + 1;
			users[uid].pid = getpid ();
			strncpy (users[uid].name, "(no name)", NAME_SIZE);
			strncpy (users[uid].ip, inet_ntoa (addr->sin_addr), 16);
			users[uid].port = addr->sin_port;
			/* broadcast that you're in */
			snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' entered from %s/%d. ***\n", users[uid].name, users[uid].ip, users[uid].port);
			broadcast (msg);
			return uid;
		}
	}
	fputs ("The server is full.\nPlease try again later...\n", stderr);
	return -1;
}

int readline (char *line, int *connection)
{
	int	i;
	char	*p = NULL;
re_read:
	p = fgets (line, MAX_LINE_SIZE + 1, stdin);
	if (ferror (stdin)) {		/* error */
		clearerr (stdin);
		if (errno == EINTR)
			goto re_read;
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

void clear_nps (Npipe **np)
{
	if (*np) {	/* if the Npipes exist */
		int	 i;
		for (i = 0; i < MAX_PIPE; ++i) {
			if ((*np)[i].fd) {	/* if the numbered pipe exists */
				close ((*np)[i].fd[0]);	/* close the opened pipe */
				close ((*np)[i].fd[1]);
				free ((*np)[i].fd);	/* free the allocated space */
				(*np)[i].fd = NULL;	/* reset to NULL */
			}
		}
		free (*np);
		*np = NULL;
	}
}

void close_np (Npipe *np)
{
	if (np && np[0].fd) {	/* if the numbered pipe to be received from exists */
		close (np[0].fd[1]);	/* close the opened pipe */
		close (np[0].fd[0]);
		free (np[0].fd);	/* free the allocated space */
		np[0].fd = NULL;	/* reset to NULL */
	}
}

void np_countdown (Npipe *np)
{
	if (np) {
		int	i;
		for (i = 0; i < MAX_PIPE - 1; ++i)	/* countdown all the numbered pipe */
			np[i] = np[i + 1];
		np[MAX_PIPE - 1].fd = NULL;	/* set the new one to NULL */
	}
}

void set_np_in (Npipe *np)
{
	if (np && np[0].fd) {
		close (np[0].fd[1]);			/* close the write end of the pipe */
		dup2 (np[0].fd[0], STDIN_FILENO);	/* duplicate the read end of the pipe to stdin */
		close (np[0].fd[0]);			/* then close the read end of the pipe */
		free (np[0].fd);			/* free the allocated space for fds */
		np[0].fd = NULL;			/* reset to NULL */
	}
}

void set_np_out (char *line, Npipe **np, int *connection)
{
	int	i, number;

	if (line[0] == 'y' && line[1] == 'e' && line[2] == 'l' && line[3] == 'l' && isspace (line[4]))
		return;
	if (line[0] == 't' && line[1] == 'e' && line[2] == 'l' && line[3] == 'l' && isspace (line[4]))
		return;
	/* find the numbered pipe in the current input line and add it in */
	for (i = 0; line[i] != 0; ++i) {
		if ((line[i] == '|' || line[i] == '!') && isnumber (line + i + 1)) {
			/* construct the numbered pipe when first use */
			if (*np == NULL)
				*np = calloc (MAX_PIPE, sizeof (Npipe));

			/* the pipe number */
			number = atoi (line + i + 1);

			/* create a new pipe if it has not been created */
			if ((*np)[number].fd == NULL) {
				(*np)[number].fd = (int *) malloc (2 * sizeof(int));
				if (pipe ((*np)[number].fd) < 0) {
					fputs ("server error: failed to create numbered pipes\n", stderr);
					*connection = 0;
				}
			}

			/* set up the output to pipe */
			dup2 ((*np)[number].fd[1], STDOUT_FILENO);
			if (line[i] == '!')
				dup2 ((*np)[number].fd[1], STDERR_FILENO);

			line[i] = 0;
			break;
		}
	}
}

void execute_one_line (int progc, char **cmds, int *connection)
{
	pid_t	childpid;
	int	i, argc, pipefd[2], stdfd[3], stat, ofd, to, from;
	char	ori_cmd[MAX_CMD_SIZE + 1] = {0};
	char	*argv[MAX_CMD_SIZE / 2 + 1] = {0};
	char	*in_file = NULL, *out_file = NULL;

	/* save original fds, may the duplicated by numbered pipes */
	save_fds (stdfd);

	for (i = 0; i < progc; ++i) {
		/* save the original command */
		strncpy (ori_cmd, cmds[i], MAX_CMD_SIZE + 1);

		/* resolve user pipes from commmand */
		if (resolv_ups (cmds[i], &ofd, &to, &from) < 0)
			break;

		/* parse the input command into argv */
		argc = cmd_to_argv (cmds[i], argv, &in_file, &out_file);

		/* set up pipes to read from */
		set_pipes_in (pipefd, i);
		set_up_in (&from, ori_cmd);

		/* execute the command accordingly */
		if (strcmp (argv[0], "exit") == 0) {
			*connection = 0;
		} else if (strcmp (argv[0], "printenv") == 0) {
			printenv (argc, argv);
		} else if (strcmp (argv[0], "setenv") == 0) {
			setupenv (argc, argv);
		} else if (strcmp (argv[0], "who") == 0) {
			who ();
		} else if (strcmp (argv[0], "name") == 0) {
			name (argv[1]);
		} else if (strcmp (argv[0], "tell") == 0) {
			tell (argc, argv);
		} else if (strcmp (argv[0], "yell") == 0) {
			yell (argc, argv);
		} else {
			/* create a pipe */
			if (i != progc - 1 && pipe (pipefd) < 0) {
				fputs ("server error: failed to create pipes\n", stderr);
				*connection = 0;
				return;
			}
			/* fork a child and exec the program */
			if ((childpid = fork()) < 0) {
				close (pipefd[0]); close (pipefd[1]);
				fputs ("server error: fork failed\n", stderr);
				i = progc;
			} else if (childpid == 0) {
				/* set up pipe to write to */
				set_pipes_out (pipefd, stdfd, i, progc);
				set_up_out (&to, &ofd, ori_cmd);
				open_files (in_file, out_file);
				/* invoke the command */
				execvpe (*argv, argv, environ);
				/* error handling */
				fprintf (stderr, "Unknown command: [%s].\n", *argv);
				*connection = -1;
				i = progc;
			} else {
				wait (&stat);
				if (stat != 0) {
					i = progc;
					if (pipefd[0])
						close (pipefd[0]);
					if (pipefd[1])
						close (pipefd[1]);
				}
			}
		}

		/* close open fifo fds, remove the input fifo */
		clear_fifo (&to, &from, &ofd);

		/* free the allocated space of one command */
		clear_argv (argc, argv, &in_file, &out_file);
	}

	/* restore original fds */
	restore_fds (stdfd);
}

void clear_fifo (int *to, int *from, int *ofd)
{
	if (*to)
		close (*ofd);
	if (*from > 0 && users[uid].fifo[*from - 1].fd) {
		unlink (users[uid].fifo[*from - 1].name);
		memset (&users[uid].fifo[*from - 1], 0, sizeof (FIFO));
	}
}

void set_up_out (int *to, int *ofd, char *ori_cmd)
{
	char	msg[MAX_MSG_SIZE + 1] = {0};
	/* set up user pipes to pipe to others */
	if (*to) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", users[uid].name, users[uid].id, ori_cmd, users[*to - 1].name, *to);
		broadcast (msg);
		dup2 (*ofd, STDOUT_FILENO);
		close (*ofd);
		*to = 0;
	}
}

void set_up_in (int *from, char *ori_cmd)
{
	char	msg[MAX_MSG_SIZE + 1] = {0};
	/* set up user pipes to receive from others */
	if (*from) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", users[uid].name, users[uid].id, users[*from - 1].name, *from, ori_cmd);
		broadcast (msg);
		dup2 (users[uid].fifo[*from - 1].fd, STDIN_FILENO);
		close (users[uid].fifo[*from - 1].fd);
	}
}

int open_up_out (int *ofd, int *to)
{
	char	msg[MAX_MSG_SIZE + 1], fifo[FIFO_NAME_SIZE + 1];
	/* check if the client is on line */
	if (users[*to - 1].id == 0) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: user #%d does not exist yet. ***\n", *to);
		write (STDERR_FILENO, msg, strlen (msg));
		*to = 0;
		return -1;
	/* check the existence of the fifo */
	} else if (users[*to - 1].fifo[uid].fd != 0) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the pipe #%d->#%d already exists. ***\n", users[uid].id, *to);
		write (STDERR_FILENO, msg, strlen (msg));
		*to = 0;
		return -1;
	}
	/* make up the fifo name */
	strncpy (msg, base_dir, FIFO_NAME_SIZE + 1);
	strcat (msg, "/fifo/%d->%d");
	snprintf (fifo, FIFO_NAME_SIZE + 1, msg, users[uid].id, *to);
	/* create the fifo */
	if (mkfifo (fifo, 0600) < 0) {
		snprintf (msg, MAX_MSG_SIZE + 1, "error: failed to create FIFO\n");
		write (STDERR_FILENO, msg, strlen (msg));
		*to = 0;
		return -1;
	} else {
		/* open the fifo to write to */
		strncpy (users[*to - 1].fifo[uid].name, fifo, FIFO_NAME_SIZE + 1);
		kill (users[*to - 1].pid, SIGUSR2);
		*ofd = open (fifo, O_WRONLY);
	}
	return 0;
}

int open_up_in (int *from)
{
	char	msg[MAX_MSG_SIZE + 1];
	/* check the client is on line */
	if (users[*from - 1].id == 0) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: user #%d does not exist yet. ***\n", *from);
		write (STDERR_FILENO, msg, strlen (msg));
		*from = 0;
		return -1;
	/* check the existence of the fifo */
	} else if (users[uid].fifo[*from - 1].fd == 0) {
		snprintf (msg, MAX_MSG_SIZE + 1, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", *from, users[uid].id);
		write (STDERR_FILENO, msg, strlen (msg));
		*from = 0;
		return -1;
	}
	return 0;
}

int resolv_ups (char *cmd, int *ofd, int *to, int *from)
{
	int	i, j;
	char	remain[MAX_CMD_SIZE + 1];
	/* initialize the values */
	*ofd = *to = *from = 0;
	/* check if it's yell command */
	if (cmd[0] == 'y' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'l' && isspace (cmd[4]))
		return 0;
	if (cmd[0] == 't' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'l' && isspace (cmd[4]))
		return 0;
	/* resolve the input command and remove the user pipes' part */
	for (i = 0; cmd[i] != 0; ++i) {
		if ((cmd[i] == '>' || cmd[i] == '<') && !isspace(cmd[i + 1])) {
			for (j = i + 1; isdigit (cmd[j]); ++j);
			if (cmd[j] == 0 || isspace (cmd[j])) {
				strncpy (remain, cmd + j, MAX_CMD_SIZE + 1);
				cmd[j] = 0;
				/* pipe to another user */
				if (cmd[i] == '>' && (*to = atoi (cmd + i + 1)) > 0) {	/* check if it's a valid id */
					if (open_up_out (ofd, to) < 0)
						return -1;
				/* receive pipe from another user */
				} else if ((*from = atoi (cmd + i + 1)) > 0) {	/* check if it's a valid id */
					if (open_up_in (from) < 0)
						return -1;
				}
				cmd[i] = ' ';
				cmd[i + 1] = 0;
				strcat (cmd, remain);
			}
		}
	}
	return 0;
}

void tell (int argc, char **argv)
{
	int	i, to = atoi (argv[1]);
	char	*msg = malloc (strlen (users[uid].name) + 19 + MAX_MSG_SIZE + 1);
	if (to > 0 && users[to - 1].id) {
		sprintf (msg, "*** %s told you ***: ", users[uid].name);
		for (i = 2; i < argc; ++i) {
			strcat (msg, argv[i]);
			if (i == argc - 1)
				strcat (msg, "\n");
			else
				strcat (msg, " ");
		}
		for (i = 0; i < MAX_MSG_NUM; ++i) {
			if (users[to - 1].msg[uid][i][0] == 0) {
				strncpy (users[to - 1].msg[uid][i], msg, MAX_MSG_SIZE + 1);
				kill (users[to - 1].pid, SIGUSR1);
				break;
			}
		}
	} else {
		sprintf (msg, "*** Error: user #%d does not exist yet. ***\n", to);
		write (STDERR_FILENO, msg, strlen (msg));
	}
	free (msg);
}

void yell (int argc, char **argv)
{
	int	i;
	char	*msg = malloc (strlen (users[uid].name) + 17 + MAX_MSG_SIZE + 1);
	sprintf (msg, "*** %s yelled ***: ", users[uid].name);
	for (i = 1; i < argc; ++i) {
		strcat (msg, argv[i]);
		if (i == argc - 1)
			strcat (msg, "\n");
		else
			strcat (msg, " ");
	}
	broadcast (msg);
	free (msg);
}

void name (char *new_name)
{
	int	idx;
	char	msg[MAX_MSG_SIZE + 1];
	for (idx = 0; idx < MAX_USERS; ++idx) {
		if (users[idx].id > 0 && idx != uid && strcmp (users[idx].name, new_name) == 0) {
			snprintf (msg, MAX_MSG_SIZE + 1, "*** User '%s' already exists. ***\n", new_name);
			write (STDOUT_FILENO, msg, strlen (msg));
			return;
		}
	}
	strncpy (users[uid].name, new_name, NAME_SIZE + 1);
	snprintf (msg, MAX_MSG_SIZE + 1, "*** User from %s/%d is named '%s'. ***\n", users[uid].ip, users[uid].port, users[uid].name);
	broadcast (msg);
}

void who (void)
{
	int	idx;
	char	msg[MAX_MSG_SIZE + 1];
	snprintf (msg, MAX_MSG_SIZE + 1, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
	write (STDOUT_FILENO, msg, strlen (msg));
	for (idx = 0; idx < MAX_USERS; ++idx) {
		if (users[idx].id > 0) {
			if (strlen (users[idx].name) < 8)
				snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t\t%s/%d", users[idx].id, users[idx].name, users[idx].ip, users[idx].port);
			else
				snprintf (msg, MAX_MSG_SIZE + 1, "%d\t%s\t%s/%d", users[idx].id, users[idx].name, users[idx].ip, users[idx].port);
			if (idx == uid)
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
	if (index == progc - 1) {
		dup2 (stdfd[1], STDOUT_FILENO);
	} else {
		dup2 (pipefd[1], STDOUT_FILENO);
		close (pipefd[0]);
		close (pipefd[1]);
		pipefd[0] = pipefd[1] = 0;
	}
}

void set_pipes_in (int *pipefd, int index)
{
	if (index != 0) {
		dup2 (pipefd[0], STDIN_FILENO);
		close (pipefd[0]);
		close (pipefd[1]);
		pipefd[0] = pipefd[1] = 0;
	}
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
	int	argc = 0, i;
	char	*arg;

	if (cmd[0] == 'y' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'l' && isspace (cmd[4])) {
		cmd[4] = 0;
		argv[0] = (char *) malloc (5);
		strcpy (argv[0], cmd);
		argv[1] = (char *) malloc (strlen (cmd + 5) + 1);
		strcpy (argv[1], cmd + 5);
		cmd[4] = ' ';
		return 2;
	} else if (cmd[0] == 't' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'l' && isspace (cmd[4])) {
		for (i = 5; isdigit (cmd[i]); ++i);
		if (isspace (cmd[i])) {
			cmd[4] = 0;
			argv[0] = (char *) malloc (5);
			strcpy (argv[0], cmd);
			cmd[i] = 0;
			argv[1] = (char *) malloc (strlen (cmd + 5) + 1);
			strcpy (argv[1], cmd + 5);
			argv[2] = (char *) malloc (strlen (cmd + i + 1) + 1);
			strcpy (argv[2], cmd + i + 1);
			cmd[4] = cmd[i] = ' ';
			return 3;
		}
	}
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

	if (line[0] == 'y' && line[1] == 'e' && line[2] == 'l' && line[3] == 'l' && isspace (line[4])) {
		cmds[0] = (char *) malloc (strlen (line) + 1);
		strcpy (cmds[0], line);
		return 1;
	} else if (line[0] == 't' && line[1] == 'e' && line[2] == 'l' && line[3] == 'l' && isspace (line[4])) {
		cmds[0] = (char *) malloc (strlen (line) + 1);
		strcpy (cmds[0], line);
		return 1;
	}
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

void sig_handler (int sig)
{
	if (sig == SIGUSR1) {	/* receive messages from others */
		int	i, j;
		for (i = 0; i < MAX_USERS; ++i) {
			for (j = 0; j < MAX_MSG_NUM; ++j) {
				if (users[uid].msg[i][j][0] != 0) {
					write (STDOUT_FILENO, users[uid].msg[i][j], strlen (users[uid].msg[i][j]));
					memset (users[uid].msg[i][j], 0, MAX_MSG_SIZE);
				}
			}
		}
	} else if (sig == SIGUSR2) {	/* open fifos to read from */
		int	i;
		for (i = 0; i < MAX_USERS; ++i) {
			if (users[uid].fifo[i].fd == 0 && users[uid].fifo[i].name[0] != 0)
				users[uid].fifo[i].fd = open (users[uid].fifo[i].name, O_RDONLY | O_NONBLOCK);
		}
	} else if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
		rm_user ();
		shmdt (users);
	}
	signal (sig, sig_handler);
}

void initialize (void)
{
	char	*wd = malloc (strlen (base_dir) + 5);

	/* get the shared memory for users */
	if ((shmid = shmget (SHMKEY, MAX_USERS * sizeof (User), PERM)) < 0) {
		fputs ("server error: shmget failed\n", stderr);
		exit (1);
	}

	/* attach the allocated shared memory */
	if ((users = (User *) shmat (shmid, NULL, 0)) == (User *) -1) {
		fputs ("server error: shmat failed\n", stderr);
		exit (1);
	}

	/* initialize the original directory */
	strcpy (wd, base_dir);
	strcat (wd, "/rwg");
	chdir (wd);
	free (wd);

	/* initialize the environment variables */
	clearenv ();
	setenv ("PATH", "bin:.", 1);

	/* establish signal handlers */
	signal (SIGUSR1, sig_handler);	/* receive messages from others */
	signal (SIGUSR2, sig_handler);	/* open fifos to read from */
	signal (SIGINT, sig_handler);
	signal (SIGQUIT, sig_handler);
	signal (SIGTERM, sig_handler);

	/* print the welcome message */
	write (STDOUT_FILENO, motd, strlen(motd));
}
