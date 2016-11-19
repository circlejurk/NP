#define SERV_TCP_PORT	9527
#define MAX_LINE_SIZE	15000
#define MAX_CMD_SIZE	256
#define MAX_PIPE	1000

#define SHMKEY ((key_t) 9527)
#define PERM 0666

#define MAX_USERS	30
#define MAX_MSG_NUM	10	/* one chat buffer has at most 10 unread messages */
#define MAX_MSG_SIZE	1024	/* one message has at most 1024 bytes */
#define NAME_SIZE	20
#define FIFO_NAME_SIZE	64

extern const char motd[], prompt[], base_dir[];

typedef struct numbered_pipe {
	int	*fd;
} Npipe;

typedef struct fifo {
	int	fd;
	char	name[FIFO_NAME_SIZE + 1];
} FIFO;

typedef struct user {
	int	id;
	int	pid;
	char	name[NAME_SIZE + 1];
	char	ip[16];
	int	port;
	char	msg[MAX_USERS][MAX_MSG_NUM][MAX_MSG_SIZE + 1];
	FIFO	fifo[MAX_USERS];
} User;

int shell (struct sockaddr_in *addr);
void initialize (void);

void save_fds (int *stdfd);
void restore_fds (int *stdfd);
int readline (char *line, int *connection);
int arespace (char *s);

int isnumber (char *s);
void set_np_out (char *line, Npipe **np, int *connection);
void set_np_in (Npipe *np);
void close_np (Npipe *np);
void np_countdown (Npipe *np);
void clear_nps (Npipe **np);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

void execute_one_line (int progc, char **cmds, int *connection);
int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);
void open_files (const char *in_file, const char *out_file);
void set_pipes_out (int *pipefd, int *stdfd, int index, int progc);
void set_pipes_in (int *pipefd, int index);

void printenv (int argc, char **argv);
void setupenv (int argc, char **argv);

int add_user (struct sockaddr_in *addr);
void rm_user (void);
void broadcast (char *msg);
void who (void);
void name (char *new_name);
void yell (int argc, char **argv);
void tell (int argc, char **argv);

int resolv_ups (char *cmd, int *ofd, int *to, int *from);
int open_up_out (int *ofd, int *to);
int open_up_in (int *from);
void set_up_out (int *to, int *ofd, char *ori_cmd);
void set_up_in (int *from, char *ori_cmd);
void clear_fifo (int *to, int *from, int *ofd);
