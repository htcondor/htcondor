#if !defined(_LOG_H)
#define _LOG_H

#include "condor_xdr.h"

/* This defines a base class for "logs" of operations.  These logs indicate
   operations which have been performed on a data structure (for example
   the job queue), and the log can be "replayed" to recreate the state of
   the data structure.
*/

class LogRecord {
public:
	
	LogRecord();
	~LogRecord();
	LogRecord *get_next() { return next; }
	int get_op_type() { return op_type; }

	int Write(int fd);
	int Write(XDR *);

	int Read(int fd);
	int Read(XDR *);
	int ReadHeader(int fd);
	int ReadHeader(XDR *);
	virtual int ReadBody(int fd) {}
	virtual int ReadBody(XDR *xdrs) { return WriteBody(xdrs); }
	int ReadTail(int fd);
	int ReadTail(XDR *);

	virtual int Play() {}

protected:
	int readstring(int, char *&);
	int op_type;	/* This is the type of operation being performed */
	int body_size;

private:
	WriteHeader(int fd);
	virtual WriteBody(int fd) {}
	WriteTail(int fd);

	WriteHeader(XDR *);
	virtual WriteBody(XDR *) {}
	WriteTail(XDR *);

	LogRecord *next;
	LogRecord *prev;
};

LogRecord *ReadLogEntry(int fd);
LogRecord *InstantiateLogEntry(int fd, int type);
#endif
