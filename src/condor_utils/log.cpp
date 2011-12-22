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


 

#define _POSIX_SOURCE

#ifdef _NO_CONDOR_
#include <stdio.h> // for FILE*
#include <stdlib.h> // for malloc, free, realloc
#include <string.h> // for strdup
#include <ctype.h> // for isspace
#else
#include "condor_common.h"
#include "condor_debug.h"
#endif

#include "log.h"

class LogRecordHead: public LogRecord {
	virtual char const *get_key() {return NULL;}
};

LogRecord::LogRecord()
{
	op_type = 0;
}


LogRecord::~LogRecord()
{
}

int
LogRecord::readword(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);
	int ch;

	// ignore leading whitespace but don't pass newline
	do {
		ch = fgetc( fp );
		if( ch == EOF || ch == '\0' ) {
			free( buf );
			return( -1 );
		}
		buf[0] = ch;
	} while (isspace(buf[0]) && buf[0]!='\n' );

	// read until whitespace
	for (i = 1; !isspace(buf[i-1]); i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		ch = fgetc( fp );
		if( ch == EOF || ch == '\0' ) {
			free( buf );
			return( -1 );
		}
		buf[i] = ch;
	}

		// no input is also an error
	if( i==1 ) {
		free( buf );
		return( -1 );
	}

	buf[i-1] = '\0';
	str = strdup((char *)buf);
	free(buf);
	return i-1;
}


int
LogRecord::readline(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);
	int ch;

	// ignore one leading whitespace character but don't pass newline
	ch = fgetc( fp );
	if( ch == EOF || ch == '\0' ) {
		free( buf );
		return( -1 );
	}
	buf[0] = ch;

	// read until newline
	for (i = 1; buf[i-1]!='\n'; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		ch = fgetc( fp );
		if( ch == EOF || ch == '\0' ) {
			free( buf );
			return( -1 );
		}
		buf[i] = ch;
	}

		// treat no input as newline
	if( i==1 ) {
		free( buf );
		return( -1 );
	}

	buf[i-1] = '\0';
	str = strdup((char *)buf);
	free(buf);
	return i-1;
}

int
LogRecord::Write(FILE *fp)
{
	int rval1, rval2, rval3;
	return( ( rval1=WriteHeader(fp) )<0 || 
			( rval2=WriteBody(fp) )  <0 || 
			( rval3=WriteTail(fp) )  <0 ? -1 : rval1+rval2+rval3);
}


int
LogRecord::Read(FILE *fp)
{
	int rval1, rval2, rval3;
	return( ( rval1=ReadHeader(fp) )<0 || 
			( rval2=ReadBody(fp) )  <0 || 
			( rval3=ReadTail(fp) )  <0 ? -1 : rval1+rval2+rval3);
}

int
LogRecord::WriteHeader(FILE *fp)
{
	char op[20];
	int  len = sprintf(op, "%d ", op_type);
	return( fprintf(fp, "%s", op) < len ? -1 : len );
}


int
LogRecord::WriteTail(FILE *fp)
{
	return( fprintf(fp, "\n") < 1 ? -1 : 1 );
}



int
LogRecord::ReadHeader(FILE *fp)
{
    int rval;
    char *op = NULL;

    rval = readword(fp, op);
    if (rval < 0) {
        return rval;
    }
    op_type = atoi(op);
    free(op);
    return rval;
}


// The ReadBody() function in all of our child classes consume the newline
// at the end of every line, so we have nothing to read.
int
LogRecord::ReadTail(FILE *  /*fp*/)
{
	return( 0 );
}

LogRecord *
ReadLogEntry(FILE *fp, LogRecord* (*InstantiateLogEntry)(FILE *fp, int type))
{
	LogRecord		*log_rec;
	LogRecordHead	head_only;
	int				rval;

	rval = head_only.ReadHeader(fp);
	if (rval < 0) {
		return 0;
	}
	log_rec = InstantiateLogEntry(fp, head_only.get_op_type());
	if (head_only.ReadTail(fp) < 0) {
		delete log_rec;
		return 0;
	}
	return log_rec;
}
