GCC = g++
FLG = -g -Wall -I../include

all: match cxi htb

match: match.C
	$(GCC) $(FLG) -o match match.C ../lib/libclassads.a

cxi: cxi.C
	$(GCC) $(FLG) -o cxi cxi.C ../lib/libclassads.a

htb: htb.C
	$(GCC) $(FLG) -o htb htb.C ../lib/libclassads.a


clean:
	rm -f *.o core *~ htb cxi match *.log
