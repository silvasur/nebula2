CC=gcc
CFLAGS=-Wall -Werror -pedantic
LIBS=-lpthread

nebula2: nebula2.o config.o render.o statefile.o mutex_helpers.o iniparser/libiniparser.a
	$(CC) $(CFLAGS) $(LIBS) -o nebula2 nebula2.o config.o render.o statefile.o mutex_helpers.o iniparser/libiniparser.a

iniparser/libiniparser.a:
	make -C iniparser libiniparser.a

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o

nuke: clean
	rm -f nebula2
