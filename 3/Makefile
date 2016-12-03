############################################
# Makefile for NP projects
# kyechou (niorehkids)
############################################ 

BUILD_DIR=build
SRC_DIR=src
HTML_DIR=public_html

CC=gcc
CFLAGS=-c -Wall -Wextra -Wpedantic -O3 -ansi
LFLAGS=-static -fPIC

all:	${HTML_DIR}/hw3.cgi ${HTML_DIR}/simple-httpd

${HTML_DIR}/%:	${BUILD_DIR}/%.o
	${CC} ${LFLAGS} -o $@ $^

${BUILD_DIR}/%.o:	${SRC_DIR}/%.c
	-@[[ -d ${BUILD_DIR} ]] || mkdir ${BUILD_DIR}
	${CC} ${CFLAGS} -o $@ $^

clean:
	-@rm -rf ${BUILD_DIR} ${HTML_DIR}/hw3.cgi ${HTML_DIR}/simple-httpd

.PHONY:	all clean
