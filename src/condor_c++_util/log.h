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
	virtual ~LogRecord();
	LogRecord *get_next() { return next; }
	int get_op_type() { return op_type; }

	int Write(int fd);
	int Write(XDR *);
	int Write(FILE *fp);

	int Read(int fd);
	int Read(XDR *);
	int Read(FILE *fp);
	int ReadHeader(int fd);
	int ReadHeader(XDR *);
	int ReadHeader(FILE *fp);
	virtual int ReadBody(int fd) {return 0;}
	virtual int ReadBody(XDR *xdrs) { return WriteBody(xdrs); }
	virtual int ReadBody(FILE *fp) {return 0;}
	int ReadTail(int fd);
	int ReadTail(XDR *);
	int ReadTail(FILE *fp);

	virtual int Play() {return 0;}

protected:
	int readstring(int, char *&);
	int readstring(FILE *fp, char *&);
	int op_type;	/* This is the type of operation being performed */
	int body_size;

private:
	int WriteHeader(int fd);
	virtual int WriteBody(int fd) {return 0;}
	int WriteTail(int fd);

	int WriteHeader(XDR *);
	virtual int WriteBody(XDR *) {return 0;}
	int WriteTail(XDR *);

	int WriteHeader(FILE *fp);
	virtual int WriteBody(FILE *) {return 0;}
	int WriteTail(FILE *fp);

	LogRecord *next;
	LogRecord *prev;
};

LogRecord *ReadLogEntry(int fd);
LogRecord *InstantiateLogEntry(int fd, int type);

LogRecord *ReadLogEntry(FILE *fp);
LogRecord *InstantiateLogEntry(FILE *fp, int type);
#endif
