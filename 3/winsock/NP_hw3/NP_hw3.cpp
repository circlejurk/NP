#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <process.h>
#include <vector>

using namespace std;

#include "resource.h"
#define R_OK 4
#define W_OK 2
#define F_OK 0
#define SERVER_PORT 80
#define WM_SOCK_NOTIFY (WM_USER + 1)
#define WM_HOST_NOTIFY (WM_USER + 2)
#define MAX_BUF_SIZE 3000

typedef struct host {
	struct sockaddr_in	sin;	/* the server socket address */
	SOCKET			sock;	/* the connection socket */
	FILE			*fin;	/* the batch file for host server input */
	int			stat;	/* 0: not connected */
					/* 1: connected but not writable */
					/* 2: connected and writable */
} Host;

struct http_cli {
	SOCKET	sock;
	Host	hosts[5];
	char	*method;
	char	*path;
	char	*qstring;
	char	*proto;
	int	stat;	/* 0: haven't been read */
			/* 1: haven't been written (file) */
			/* 2: haven't been written (cgi) */
			/* 3: remote batch */
			/* -1: have been disconnected */
};

vector<struct http_cli>	clients;

/************ the cgi part ************/

void exec_hw3_cgi (int i);
int resolv_requests (int i);
int add_host (Host *host, char *hostname, char *port, char *filename);
void preoutput (int ci);

void exec_hw3_cgi (int i)
{
	clients[i].stat = 3;
	if (resolv_requests (i) < 0)
		return;
	preoutput (i);
}

void preoutput (int ci)
{
	int	i;
	char	buf[MAX_BUF_SIZE + 1] = {0}, tmp[64];
	strncpy (buf, "Content-Type: text/html\n\n", MAX_BUF_SIZE + 1);
	strncat (buf, "<html>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<head>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<title>Network Programming Homework 3</title>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "</head>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<body bgcolor=#336699>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<font face=\"Courier New\" size=2 color=#FFFF99>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<table width=\"800\" border=\"1\">\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	for (i = 0; i < 5; ++i) {
		strncat (buf, "<td>", MAX_BUF_SIZE + 1 - strlen (buf));
		if (clients[ci].hosts[i].sock)
			strncat (buf, inet_ntoa (clients[ci].hosts[i].sin.sin_addr), MAX_BUF_SIZE + 1 - strlen (buf));
		strncat (buf, "</td>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	}
	strncat (buf, "</tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "<tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	for (i = 0; i < 5; ++i) {
		sprintf (tmp, "<td valign=\"top\" id=\"m%d\"></td>\n", i);
		strncat (buf, tmp, MAX_BUF_SIZE + 1 - strlen (buf));
	}
	strncat (buf, "</tr>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	strncat (buf, "</table>\n", MAX_BUF_SIZE + 1 - strlen (buf));
	send (clients[ci].sock, buf, sizeof (buf), 0);
}

int resolv_requests (int ci)
{
	int	i;
	char	*token, hostname[64], port[10], filename[32];

	token = strtok (clients[ci].qstring, "&");
	while (token != NULL) {
		/* reset to empty string */
		hostname[0] = port[0] = filename[0] = 0;
		/* read the hostname or IP address */
		sscanf (token, "h%d=%s", &i, hostname);
		token = strtok (NULL, "&");
		/* read the port of the service */
		sscanf (token, "p%d=%s", &i, port);
		token = strtok (NULL, "&");
		/* read the filename of the batch input */
		sscanf (token, "f%d=%s", &i, filename);
		token = strtok (NULL, "&");

		/* check input validity */
		if (*hostname == 0 || *port == 0 || *filename == 0)
			continue;

		if (add_host (&clients[ci].hosts[i - 1], hostname, port, filename) < 0) {
			fprintf (stderr, "error: add_host failed at hosts[%d]\n", i - 1);
			return -1;
		}
	}

	return 0;
}

int add_host (Host *host, char *hostname, char *port, char *filename)
{
	int		err;
	struct hostent	*phe;

	/* set up server socket addr */
	memset (&host->sin, 0, sizeof (host->sin));
	host->sin.sin_family = AF_INET;
	host->sin.sin_port = htons (atoi (port));
	if ((phe = gethostbyname (hostname)))
		host->sin.sin_addr = *((struct in_addr *) phe->h_addr_list[0]);
	else if ((host->sin.sin_addr.s_addr = inet_addr (hostname)) == INADDR_NONE) {
		fprintf (stderr, "error: cannot get the hostname '%s'\n", hostname);
		return -1;
	}

	/* allocate the socket */
	if ((host->sock = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		fputs ("error: failed to build a socket\n", stderr);
		return -1;
	}

	/* open the input file */
	if ((host->fin = fopen (filename, "r")) == NULL) {
		fprintf (stderr, "error: failed to open file '%s'\n", filename);
		return -1;
	}

	/* set the status of the host to 'not connected' */
	host->stat = 0;

	return 0;
}

/************ the httpd part ************/

int EditPrintf (HWND, TCHAR *, ...);
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);
BOOL CALLBACK MainDlgProc (HWND, UINT, WPARAM, LPARAM);
BOOL passiveTCP (int port, int qlen, HWND hwnd, HWND hwndEdit, SOCKET *msock);
void read_req (int i);
void http_out (int i);
void clear_cli (int i);
void OK (int i);
void notfound (int i);
void readfile (int i);

struct arg {
	HWND	hwnd;
	size_t	i;
};

BOOL CALLBACK MainDlgProc (HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	static HWND		hwndEdit;
	static SOCKET		msock;
	static struct http_cli	client;
	WSADATA			wsaData;
	//static struct arg	*Arg = (struct arg *) malloc (sizeof (struct arg));

	switch (Message)
	{
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem (hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_LISTEN:
					WSAStartup (MAKEWORD(2, 0), &wsaData);
					return passiveTCP (SERVER_PORT, 0, hwnd, hwndEdit, &msock);
					break;
				case ID_EXIT:
					EndDialog (hwnd, 0);
					break;
			};
			break;
		case WM_CLOSE:
			EndDialog (hwnd, 0);
			break;
		case WM_SOCK_NOTIFY:
			switch(WSAGETSELECTEVENT(lParam))
			{
				case FD_ACCEPT:
					memset (&client, 0, sizeof (struct http_cli));
					client.sock = accept(msock, NULL, NULL);
					clients.push_back (client);
					EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), client.sock, clients.size());
					break;
				case FD_READ:
					for (size_t i = 0; i < clients.size(); ++i)
						read_req (i);
					break;
				case FD_WRITE:
					for (size_t i = 0; i < clients.size(); ++i) {
						_beginthread ((void (*) (void *)) http_out, 0, (void *) i);
					}
					break;
				case FD_CLOSE:
					for (size_t i = 0; i < clients.size(); ++i)
						clear_cli (i);
					break;
			};
			break;
		case WM_HOST_NOTIFY:
			switch(WSAGETSELECTEVENT(lParam))
			{
				case FD_CONNECT:
					EditPrintf(hwndEdit, TEXT("=== CONNECT ===\r\n"));
					break;
				case FD_READ:
					EditPrintf(hwndEdit, TEXT("=== READ ===\r\n"));
					break;
				case FD_WRITE:
					EditPrintf(hwndEdit, TEXT("=== WRITE ===\r\n"));
					break;
				case FD_CLOSE:
					EditPrintf(hwndEdit, TEXT("=== CLOSE ===\r\n"));
					break;
			};
			break;

		default:
			return FALSE;
	};

	return TRUE;
}

void http_out (int i)
{
	if (clients[i].stat != 1 && clients[i].stat != 2)
		return;
	if (clients[i].stat == 2) {
		/* execute the hw3.cgi codes */
		OK (i);
		exec_hw3_cgi (i);
		return;
	}
	if (clients[i].stat == 1 && _access (clients[i].path, F_OK|R_OK) != -1) {
		/* retrieve the form_get.htm */
		OK (i);
		readfile (i);
	} else {
		notfound (i);	/* 404 */
	}
	clients[i].stat = -1;
	clear_cli (i);
}

void readfile (int i)
{
	FILE	*fin = fopen (clients[i].path, "r");
	char	buf[MAX_BUF_SIZE + 1];
	strncpy (buf, "Content-Type: text/html\n\n", MAX_BUF_SIZE + 1);
	send (clients[i].sock, buf, strlen (buf), 0);
	while (fgets (buf, MAX_BUF_SIZE + 1, fin))
		send (clients[i].sock, buf, strlen (buf), 0);
}

void read_req (int i)
{
	int	cc;
	char	buf[MAX_BUF_SIZE + 1], *p, *token;
	if (clients[i].stat != 0)
		return;
	cc = recv (clients[i].sock, buf, MAX_BUF_SIZE + 1, 0);
	if (cc != SOCKET_ERROR && cc > 0) {
		for (p = buf; *p != '\n' && *p != '\r'; ++p);
		*p = 0;
		/* method */
		token = strtok (buf, " \r\n");
		clients[i].method = (char *) malloc (strlen (token) + 1);
		strncpy (clients[i].method, token, strlen (token) + 1);
		/* path */
		token = strtok (NULL, " \r\n");
		for (p = token; *p != '?' && *p != 0; ++p);
		if (*p == '?') {
			*p = 0;
			++p;
		}
		clients[i].path = (char *) malloc (strlen (token) + 1);
		strncpy (clients[i].path, token + 1, strlen (token) + 1);
		/* query string */
		clients[i].qstring = (char *) malloc (strlen (p) + 1);
		strncpy (clients[i].qstring, p, strlen (p) + 1);
		/* HTTP protocol */
		token = strtok (NULL, " \r\n");
		clients[i].proto = (char *) malloc (strlen (token) + 1);
		strncpy (clients[i].proto, token, strlen (token) + 1);
		/* status */
		if (strcmp (clients[i].path, "hw3.cgi") == 0)
			clients[i].stat = 2;
		else
			clients[i].stat = 1;
	} else if (cc == 0) {
		clients[i].stat = -1;
	}
}

void clear_cli (int i)
{
	if (clients[i].stat != -1)
		return;
	free (clients[i].method);
	free (clients[i].path);
	free (clients[i].qstring);
	free (clients[i].proto);
	closesocket (clients[i].sock);
	clients.erase (clients.begin() + i);
}

void OK (int i)
{
	char	buf[MAX_BUF_SIZE + 1];
	strncpy (buf, clients[i].proto, MAX_BUF_SIZE + 1);
	strncat (buf, " 200 OK\n"
		"Server: sake\n", MAX_BUF_SIZE + 1 - strlen (clients[i].proto));
	send (clients[i].sock, buf, strlen (buf), 0);
}

void notfound (int i)
{
	char	buf[MAX_BUF_SIZE + 1];
	strncpy (buf, clients[i].proto, MAX_BUF_SIZE + 1);
	strncat (buf, " 404 Not Found\n"
		"Server: sake\n"
		"Content-Type: text/html\n\n"
		"<html>\n"
		"<head><title>404 Not Found</title></head>\n"
		"<body><h1>404 Not Found</h1></body>\n"
		"</html>\n", MAX_BUF_SIZE + 1 - strlen (clients[i].proto));
	send (clients[i].sock, buf, strlen (buf), 0);
}

BOOL passiveTCP (int port, int qlen, HWND hwnd, HWND hwndEdit, SOCKET *msock)
{
	int		err;
	static struct	sockaddr_in sa;

	/* create master socket */
	*msock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*msock == INVALID_SOCKET) {
		EditPrintf (hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
		WSACleanup ();
		return FALSE;
	}

	/* fill the address info about server */
	sa.sin_family		= AF_INET;
	sa.sin_port		= htons (SERVER_PORT);
	sa.sin_addr.s_addr	= INADDR_ANY;
	/* bind the master socket */
	err = bind (*msock, (LPSOCKADDR)&sa, sizeof (struct sockaddr));
	if (err == SOCKET_ERROR) {
		EditPrintf (hwndEdit, TEXT("=== Error: binding error ===\r\n"));
		WSACleanup ();
		return FALSE;
	}

	/* listen to the bound address */
	err = listen (*msock, 2);
	if (err == SOCKET_ERROR) {
		EditPrintf (hwndEdit, TEXT("=== Error: listen error ===\r\n"));
		WSACleanup ();
		return FALSE;
	}

	/* async select for msock */
	err = WSAAsyncSelect (*msock, hwnd, WM_SOCK_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);
	if (err == SOCKET_ERROR) {
		EditPrintf (hwndEdit, TEXT("=== Error: select error ===\r\n"));
		closesocket (*msock);
		WSACleanup();
		return FALSE;
	}

	EditPrintf (hwndEdit, TEXT("=== Server START ===\r\n"));
	return TRUE;
}

int EditPrintf (HWND hwndEdit, TCHAR * szFormat, ...)
{
	TCHAR   szBuffer [1024] ;
	va_list pArgList ;

	va_start (pArgList, szFormat) ;
	wvsprintf (szBuffer, szFormat, pArgList) ;
	va_end (pArgList) ;

	SendMessage (hwndEdit, EM_SETSEL, (WPARAM) -1, (LPARAM) -1);
	SendMessage (hwndEdit, EM_REPLACESEL, FALSE, (LPARAM) szBuffer);
	SendMessage (hwndEdit, EM_SCROLLCARET, 0, 0);
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}
