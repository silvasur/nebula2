CC=gcc
CFLAGS=-Wall -Werror -pedantic
LIBS=-lpthread
# (Optimization) flags as suggested by SFMT docu.
SFMTFLAGS=-DHAVE_SSE2 -DSFMT_MEXP=19937
OPTIMIZE=-O3 -fno-strict-aliasing
#OPTIMIZE=-ggdb

nebula2: nebula2.o config.o render.o statefile.o color.o mutex_helpers.o iniparser/libiniparser.a SFMT/SFMT.c
	$(CC) $(CFLAGS) $(OPTIMIZE) $(SFMTFLAGS) $(LIBS) -o nebula2 nebula2.o config.o render.o statefile.o color.o mutex_helpers.o iniparser/libiniparser.a SFMT/SFMT.c

iniparser/libiniparser.a:
	make -C iniparser libiniparser.a

%.o:%.c
	$(CC) $(CFLAGS) $(OPTIMIZE) $(SFMTFLAGS) -c -o $@ $<

clean:
	rm -f *.o

nuke: clean
	rm -f nebula2
