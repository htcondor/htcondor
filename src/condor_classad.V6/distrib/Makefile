GCC = g++

all: htb cxi

cxi: cxi.C
	$(GCC) -Wall -I ../include -o cxi cxi.C ../lib/libclassads.a

htb: htb.C
	$(GCC) -Wall -I ../include -o htb htb.C ../lib/libclassads.a

clean:
	rm -f *.o core *~ htb cxi
