############################################
# Makefile for NP projects
# kyechou (niorehkids)
############################################ 

BUILD_DIR=build
SRC_DIR=src

CC=gcc
CFLAGS=-c -Wall -Wextra -Wpedantic -O3 -ansi
LFLAGS=

all:	socks hw4.cgi

socks: ${BUILD_DIR}/server.o ${BUILD_DIR}/socks.o
	${CC} ${LFLAGS} -o $@ $^

hw4.cgi: ${BUILD_DIR}/hw4.cgi.o
	${CC} ${LFLAGS} -o $@ $^

${BUILD_DIR}/%.o: ${SRC_DIR}/%.c
	-@[[ -d ${BUILD_DIR} ]] || mkdir ${BUILD_DIR}
	${CC} ${CFLAGS} -o $@ $^

clean:
	-@rm -rf ${BUILD_DIR} socks hw4.cgi

.PHONY:	all clean
