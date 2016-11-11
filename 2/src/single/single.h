#define SERV_TCP_PORT	9527
#define MAX_LINE_SIZE	15000
#define MAX_CMD_SIZE	256
#define MAX_PIPE	1000

#define MAX_USERS	30
#define NAME_SIZE	1024

extern const char motd[], prompt[];

typedef struct numbered_pipe {
	int	*fd;
} Npipe;

typedef struct user {
	int	sock;
	char	name[NAME_SIZE];
	char	ip[16];
	int	port;
	int	connection;
	Npipe	np[MAX_PIPE];
} User;

int shell (User *user);

void save_fds (int *stdfd);
void restore_fds (int *stdfd);
int readline (char *line, int *connection);
int arespace (char *s);

int isnumber (char *s);
void set_np_out (char *line, Npipe np[MAX_PIPE], int *connection);
void set_np_in (Npipe np[MAX_PIPE]);
void close_np (Npipe np[MAX_PIPE]);
void np_countdown (Npipe np[MAX_PIPE]);
void clear_nps (Npipe np[MAX_PIPE]);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

void execute_one_line (int progc, char **cmds, int *connection);
int cmd_to_argv (char *cmd, char **argv, char **in_file, char **out_file);
void clear_argv (int argc, char **argv, char **in_file, char **out_file);
void open_files (const char *in_file, const char *out_file);
void set_pipes_out (int *pipefd, int *stdfd, int index, int progc);
void set_pipes_in (int *pipefd, int *stdfd, int index, int progc);

void printenv (int argc, char **argv);
void setupenv (int argc, char **argv);
