#define SERV_TCP_PORT	9527
#define MAX_LINE_SIZE	15000
#define MAX_CMD_SIZE	256
#define MAX_PIPE	1000

#define MAX_USERS	30
#define MAX_MSG_SIZE	1024	/* one message has at most 1024 bytes */
#define NAME_SIZE	20

extern const char motd[], prompt[];

typedef struct numbered_pipe {
	int	*fd;
} Npipe;

typedef struct user {
	char	name[NAME_SIZE + 1];	/* client's name */
	char	ip[16];			/* client's ip */
	int	port;			/* client's port */
	int	connection;		/* client's connection status, 1: on, 0: off, -1: err */
	Npipe	*np;			/* client's numbered pipe, created when first use */
	int	*up;			/* client's user pipe, created when the user logged in */
} User;

/* implemented in shell.c */
int shell (int sock, User *users);

void save_fds (int *stdfd);
void restore_fds (int *stdfd);
int readline (char *line, int *connection);
int arespace (char *s);

int isnumber (char *s);
void set_np_out (char *line, int sock, User *users);
void set_np_in (Npipe *np);
void close_np (Npipe *np);
void np_countdown (Npipe *np);
void clear_nps (int sock, User *users);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

void execute_one_line (int progc, char **cmds, int sock, User *users);
int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);
void open_files (const char *in_file, const char *out_file);
void set_pipes_out (int *pipefd, int *stdfd, int index, int progc);
void set_pipes_in (int *pipefd, int index);

void printenv (int argc, char **argv);
void setupenv (int argc, char **argv);
void who (int sock, User *users);
void name (int sock, User *users, char *new_name);
void tell (int sock, User *users, int argc, char **argv);
void yell (int sock, User *users, int argc, char **argv);

int resolv_ups (char *cmd, int userpipe[2], int *to, int *from, int sock, User *users);
void set_up_out (int sock, User *users, int *to, int userpipe[2], char *ori_cmd);
void set_up_in (int sock, User *users, int *from, int userpipe[2], char *ori_cmd);
void clear_ups (int sock, User *users);

/* implemented in server.c */
void broadcast (char *msg, int sock, User *users);
