#if !defined(_LOG_H)
#define _LOG_H

/* 
   This defines a base class for logs of data structure operations.  
   The logs are meant to be strictly ascii (for example, no '\0' 
   characters).  A log entry is of the form: "op_type body\n" where
   op_type is the ascii decimal representation of op_type and body
   is defined by WriteBody and ReadBody for that log entry.  Users
   are encouraged to use fflush() and fsync() to commit entries to the 
   log.  The Play() method is defined to perform the operation on
   the data structure passed in as an argument.  The argument is of
   type (void *) for generality.
*/

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

	virtual int Play(void *data_structure) { return 0; }

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

LogRecord *ReadLogEntry(int fd, LogRecord* (*InstantiateLogEntry)(int fd, int type));
#endif
