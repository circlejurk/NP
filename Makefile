# Makefile for Network Programming project1

CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -O3 -ansi

all:	bin/ras-server

bin/ras-server:	src/ras-server.c
	-@mkdir bin
	${CC} ${CFLAGS} -o $@ $^

clean:
	-@rm -rf bin

.PHONY:	all clean
