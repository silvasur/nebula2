CC=gcc -Wall -Werror -pedantic -std=c99

all:



%.o:%.c
	$(CC) -c -o $@ $<

clean:
	rm -f *.o