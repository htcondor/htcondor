/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
	virtual ~LogRecord();
	LogRecord *get_next() { return next; }
	int get_op_type() { return op_type; }

	int Write(int fd);
	int Write(FILE *fp);
	int Read(int fd);
	int Read(FILE *fp);
	int ReadHeader(int fd);
	int ReadHeader(FILE *fp);
	virtual int ReadBody(int) { return 0; }
	virtual int ReadBody(FILE *) { return 0; }
	int ReadTail(int fd);
	int ReadTail(FILE *fp);

	virtual int Play(void *) { return 0; }

	static int readword(FILE*, char *&);
	static int readline(FILE*, char *&);
	static int readword(int, char *&);
	static int readline(int, char *&);

protected:
	int op_type;	/* This is the type of operation being performed */

private:
	int WriteHeader(int fd);
	int WriteHeader(FILE *fp);
	virtual int WriteBody(int) { return 0; }
	virtual int WriteBody(FILE *) { return 0; }
	int WriteTail(int fd);
	int WriteTail(FILE *fp);

	LogRecord *next;
	LogRecord *prev;
};

LogRecord *ReadLogEntry(int fd, LogRecord* (*InstantiateLogEntry)(int fd, int type));
LogRecord *ReadLogEntry(FILE* fp, LogRecord* (*InstantiateLogEntry)(FILE *fp, int type));
#endif
