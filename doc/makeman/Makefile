CC = g++
CFLAGS = -g

.C.o:
	$(CC) $(CFLAGS) -c $<

all: makeman

makeman: makeman.o
	$(CC) $(CFLAGS) -o makeman makeman.o

test: makeman
	./makeman -i test.html -o test.1

make hard: makeman
	./makeman -i hard-test.html -o hard-test.1

clean:
	rm -f *.o makeman *.out *~ test.1 hard-test.1
