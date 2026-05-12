CC = gcc
CFLAGS = -Wall -Wextra -g

myshell: shell.c
	$(CC) $(CFLAGS) -o myshell shell.c

clean:
	rm -f myshell