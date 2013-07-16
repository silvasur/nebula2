CC=gcc
CFLAGS=-Wall -Werror -pedantic -std=c99 -static
LIBS=-Liniparser -liniparser

nebula2: nebula2.o config.o iniparser/libiniparser.a
	$(CC) $(CFLAGS) -o nebula2 nebula2.o config.o iniparser/libiniparser.a

iniparser/libiniparser.a:
	make -C iniparser libiniparser.a

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o

nuke: clean
	rm -f nebula2