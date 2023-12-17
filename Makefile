CC=clang
CFLAGS=-g -Wall -pedantic -Werror -std=c99

all: pack

pack:
	tar -czvf pa1.tar.gz pa1
	tar -czvf pa2.tar.gz pa2
	tar -czvf pa3.tar.gz pa3
	tar -czvf pa4.tar.gz pa4
	tar -czvf pa5.tar.gz pa5
	tar -czvf pa6.tar.gz pa6

clean:
	rm -f lab1
	rm -f lab2
	rm -f lab3
	rm -f lab4
	rm -f lab5
	rm -f *.tar.gz

#.PHONY: all clean pa1 pa2 pa3 pa4 pack
