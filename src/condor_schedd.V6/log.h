#if !defined(_LOG_H)
#define _LOG_H

/* This defines a base class for "logs" of operations.  These logs indicate
   operations which have been performed on a data structure (for example
   the job queue), and the log can be "replayed" to recreate the state of
   the data structure.
*/

#include "condor_io.h"

class LogRecord {
public:
	
	LogRecord();
	~LogRecord();
	LogRecord *get_next() { return next; }
	int get_op_type() { return op_type; }

	int Write(int fd);
	int Write(Stream *);

	int Read(int fd);
	int Read(Stream *);
	int ReadHeader(int fd);
	int ReadHeader(Stream *);
	virtual int ReadBody(int fd) { return 0; }
	virtual int ReadBody(Stream *s) { return WriteBody(s); }
	int ReadTail(int fd);
	int ReadTail(Stream *);

	virtual int Play() { return 0; }

protected:
	int readstring(int, char *&);
	int op_type;	/* This is the type of operation being performed */
	int body_size;

private:
	int WriteHeader(int fd);
	virtual int WriteBody(int fd) { return 0; }
	int WriteTail(int fd);

	int WriteHeader(Stream *);
	virtual int WriteBody(Stream *) { return 0; }
	int WriteTail(Stream *);

	LogRecord *next;
	LogRecord *prev;
};

LogRecord *ReadLogEntry(int fd);
LogRecord *InstantiateLogEntry(int fd, int type);
#endif
