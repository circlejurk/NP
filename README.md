# NCTU Network Programming Course

The courses are lectured by Prof. I-Chen Wu.

There are four projects in this semester.
Each project is based on the previous projects.

## project 1

The first project is to make a remote shell. Many clients can connect to the
server at the same time, with the concurrent, connection-oriented model. The
server shell should support piping between programs, piping between different
lines of commands, and file redirections.

## project 2

The second project is to make a remote working ground. Extended from the first
project, each client connected to the server can see and chat with another
client, and they still be able to use the functions in the first project. Every
client can broadcast, send private messages, and pipe commands' output to
another client's command.

The second project should be implemented in two versions. One is the concurrent,
connection-oriented model with shared memory for inter-process communication,
and the other version is the single-process, concurrent model, using select()
to handle file descriptors. Both implementations should not block in any case.

## project 3

In the third project, we are asked to make a CGI program and a simple http
server.

The CGI program is given a list of server IPs, a list of server ports, and a
list of batch files containing some commands that should be sent and executed in
the server. The CGI program then connect to each server, send the batch
commands, and receive the servers' output. It should be implemented with the
single-process concurrent model, using non-blocking system calls to handle the
connections to the servers.

The http server should support CGI and follow the HTTP/1.1 standard.

The third part of the third project is to combine the CGI program and the simple
http server program to make a remote batch system, which must be implemented in
Windows using winsock libraries and event-driven model.

## project 4

The last project is to write a SOCKS server, following the [SOCK4 protocol](http://www.openssh.com/txt/socks4.protocol),
and then modify the CGI program from the third project so that it will connect
to a given SOCKS server first and let the SOCKS server direct the request to
the destination server.

The SOCKS server should be implemented with concurrent, connection-oriented
paradigm, supporting both CONNECT mode and BIND mode of the SOCKS 4 protocol,
with a simple white list firewall, and can show verbose debug messages.

