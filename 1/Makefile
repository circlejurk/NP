############################################
# Makefile for NP projects
# kyechou (niorehkids)
############################################ 

TARGET=server

BUILD_DIR=build
SRC_DIR=src

CC=gcc
CFLAGS=-c -Wall -Wextra -Wpedantic -O3 -ansi
LFLAGS=-static -fPIC

all:	${TARGET}

${TARGET}:	${BUILD_DIR}/server.o ${BUILD_DIR}/shell.o
	${CC} ${LFLAGS} -o $@ $^

${BUILD_DIR}/%.o:	${SRC_DIR}/%.c
	-@[[ -d ${BUILD_DIR} ]] || mkdir ${BUILD_DIR}
	${CC} ${CFLAGS} -o $@ $^

clean:
	-@rm -rf ${BUILD_DIR} ${TARGET}

.PHONY:	all clean
