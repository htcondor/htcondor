#if !defined(_LOG_H)
#define _LOG_H

/* This defines a base class for "logs" of operations.  These logs indicate
   operations which have been performed on a data structure (for example
   the job queue), and the log can be "replayed" to recreate the state of
   the data structure.
*/

#include "classad_hashtable.h"

class LogRecord {
public:
	
	LogRecord();
	~LogRecord();
	LogRecord *get_next() { return next; }
	int get_op_type() { return op_type; }

	int Write(int fd);
	int Write(FILE *fp);
	int Read(int fd);
	int Read(FILE *fp);
	int ReadHeader(int fd);
	int ReadHeader(FILE *fp);
	virtual int ReadBody(int fd) { return 0; }
	virtual int ReadBody(FILE *fp) { return 0; }
	int ReadTail(int fd);
	int ReadTail(FILE *fp);

	virtual int Play(ClassAdHashTable &table) { return 0; }

protected:
	int readword(int, char *&);
	int readline(int, char *&);
	int op_type;	/* This is the type of operation being performed */

private:
	int WriteHeader(int fd);
	int WriteHeader(FILE *fp);
	virtual int WriteBody(int fd) { return 0; }
	virtual int WriteBody(FILE *fp) { return 0; }
	int WriteTail(int fd);
	int WriteTail(FILE *fp);

	LogRecord *next;
	LogRecord *prev;
};

LogRecord *ReadLogEntry(int fd);
LogRecord *InstantiateLogEntry(int fd, int type);
#endif
