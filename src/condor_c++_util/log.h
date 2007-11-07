/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
	int get_op_type() { return op_type; }

	int Write(FILE *fp);
	int Read(FILE *fp);
	int ReadHeader(FILE *fp);
	virtual int ReadBody(FILE *) { return 0; }
	int ReadTail(FILE *fp);

	virtual int Play(void *) { return 0; }

	static int readword(FILE*, char *&);
	static int readline(FILE*, char *&);

	virtual char const *get_key() = 0;

protected:
	int op_type;	/* This is the type of operation being performed */

private:
	int WriteHeader(FILE *fp);
	virtual int WriteBody(FILE *) { return 0; }
	int WriteTail(FILE *fp);
};

LogRecord *ReadLogEntry(int fd, LogRecord* (*InstantiateLogEntry)(int fd, int type));
LogRecord *ReadLogEntry(FILE* fp, LogRecord* (*InstantiateLogEntry)(FILE *fp, int type));
#endif
