/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
