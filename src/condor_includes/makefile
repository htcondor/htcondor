PROGS =  test.constants test.debug test.config test.expressions test.xdr test.jobqueue test.common

All: $(PROGS)

clean: ALWAYS
	/bin/rm -f $(PROGS) *.o

test.constants: ALWAYS
	$(CC) $(CFLAGS) -o test.constants test.constants.c

test.debug: ALWAYS
	$(CC) $(CFLAGS) -o test.debug test.debug.c

test.config: ALWAYS
	$(CC) $(CFLAGS) -o test.config test.config.c

test.expressions: ALWAYS
	$(CC) $(CFLAGS) -o test.expressions test.expressions.c

test.xdr: ALWAYS
	$(CC) $(CFLAGS) -o test.xdr test.xdr.c

test.jobqueue: ALWAYS
	$(CC) $(CFLAGS) -o test.jobqueue test.jobqueue.c

test.common: ALWAYS
	$(CC) $(CFLAGS) -o test.common test.common.c

ALWAYS:
