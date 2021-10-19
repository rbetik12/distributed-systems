CC=clang
CFLAGS=-g -Wall -pedantic -Werror -std=c99

all: pa1 pa2
pa1:
	$(CC) $(CFLAGS) pa1/*.c -o lab1

pa2:
	$(CC) $(CFLAGS) pa2/*.c -o lab2 -L./pa2/ -lruntime

pack:
	tar -czvf pa1.tar.gz pa1
	tar -czvf pa2.tar.gz pa2

clean:
	rm -f lab1
	rm -f lab2
	rm -f *.tar.gz

.PHONY: all clean pa1 pa2