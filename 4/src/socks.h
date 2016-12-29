#ifndef SOCKS_H
#define SOCKS_H

#define MAX_BUF_SIZE	500
#define MAX_USER_LEN	100
#define MAX_DN_LEN	300

typedef struct SOCKS4_req {
	uint8_t		vn;
	uint8_t		cd;
	uint16_t	dest_port;
	struct in_addr	dest_ip;
	char		user[MAX_USER_LEN];
	char		dn[MAX_DN_LEN];
} Request;

typedef struct SOCKS4_rep {
	uint8_t		vn;
	uint8_t		cd;
	uint16_t	dest_port;
	struct in_addr	dest_ip;
} Reply;

int socks (struct sockaddr_in src);
int recv_req (void);
int check_fw (void);
void verbose (struct sockaddr_in *src);
int CONNECT (void);
int BIND (void);

int isnumber (char *s);
int readline (char *line, int *connection);

int line_to_cmds (char *line, char **cmds);
void clear_cmds (int progc, char **cmds);

#endif
