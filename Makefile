CC=clang
CFLAGS=-g -Wall -pedantic

SRC=$(wildcard pa1/*.c)

all:
	clang -g -Wall -Werror -pedantic -std=c99 pa1/*.c -o lab1

pack:
	tar -czvf pa1.tar.gz pa1

clean:
	rm -f lab1
	rm -f pa1.tar.gz