
CC = gcc
CFLAGS = -g -I/usr/X/include -I/usr/X11/include -I/usr/X11R6/include -I/usr/include/X11 -I/usr/include/X -I/s/X11/include

imake: imake.o
	$(CC) -o imake imake.o

imake.o: ccimake imake.c
	$(CC) -c $(CFLAGS) `./ccimake` imake.c

ccimake: ccimake.c
	$(CC) -o ccimake $(CFLAGS) ccimake.c

clean:
	rm -f core *.o *~ ccimake imake.o imake
