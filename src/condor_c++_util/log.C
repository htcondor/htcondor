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

 

#define _POSIX_SOURCE

#include "condor_common.h"

#include "log.h"

LogRecord::LogRecord()
{
}


LogRecord::~LogRecord()
{
}

int
LogRecord::readword(int fd, char * &str)
{
	int		i, rval, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace but don't pass newline
	do {
		rval = read(fd, &(buf[0]), 1);
		if (rval < 0) {
			free(buf);
			return -1;
		}
	} while( rval>0 && isspace(buf[0]) && buf[0]!='\n' );

	// read until whitespace
	for (i = 1; rval > 0 && !isspace(buf[i-1]) && buf[i-1] != '\0'; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		rval = read(fd, &(buf[i]), 1);
	}

		// check for error (no input is also an error)
	if (rval <= 0 || i==1 ) {
		free(buf);
		return -1;
	}

	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return i-1;
}

int
LogRecord::readword(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace but don't pass newline
	do {
		buf[0] = fgetc( fp );
		if( buf[0] == EOF && !feof( fp ) ) {
			free( buf );
			return( -1 );
		}
	} while (isspace(buf[0]) && buf[0]!=EOF && buf[0]!='\n' );

	// read until whitespace
	for (i = 1; !isspace(buf[i-1]) && buf[i-1]!='\0' && buf[i-1]!=EOF; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		buf[i] = fgetc( fp );
		if( buf[i] == EOF && !feof( fp ) ) {
			free( buf );
			return( -1 );
		}
	}

		// no input is also an error
	if( feof( fp ) || i==1 ) {
		free( buf );
		return( -1 );
	}

	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return i-1;
}


int
LogRecord::readline(int fd, char * &str)
{
	int		i, rval, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace but don't pass newline
	do {
		rval = read(fd, &(buf[0]), 1);
		if (rval < 0) {
			return rval;
		}
	} while( rval>0 && isspace(buf[0]) && buf[0] != '\n' );

	// read until newline
	for (i = 1; rval > 0 && buf[i-1] != '\n' && buf[i-1] != '\0'; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		}
		rval = read(fd, &(buf[i]), 1);
	}

		// report read errors, EOF and no input as errors
	if (rval <= 0 || i==1 ) {
		free(buf);
		return -1;
	}

	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return i-1;
}

int
LogRecord::readline(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);

	// ignore leading whitespace but don't pass newline
	do {
		buf[0] = fgetc( fp );
		if( buf[0] == EOF && !feof( fp ) ) {
			free( buf );
			return( -1 );
		}
	} while( isspace(buf[0]) && buf[0] != EOF && buf[0] != '\n' );

	// read until newline
	for (i = 1; buf[i-1]!='\n' && buf[i-1] != '\0' && buf[i-1] != EOF; i++) {
		if (i == bufsize) {
			buf = (char *)realloc(buf, bufsize*2);
			bufsize *= 2;
		} 
		buf[i] = fgetc( fp );
		if( buf[i] == EOF && !feof( fp ) ) {
			free( buf );
			return( -1 );
		}
	}

		// treat no input as newline
	if( feof( fp ) || i==1 ) {
		free( buf );
		return( -1 );
	}

	buf[i-1] = '\0';
	str = strdup(buf);
	free(buf);
	return i-1;
}

int
LogRecord::Write(int fd)
{
	int rval1, rval2, rval3;
	return( ( rval1=WriteHeader(fd) )<0 || 
			( rval2=WriteBody(fd) )  <0 || 
			( rval3=WriteTail(fd) )  <0 ? -1 : rval1+rval2+rval3);
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
LogRecord::Read(int fd)
{
	int rval1, rval2, rval3;
	return( ( rval1=ReadHeader(fd) )<0 || 
			( rval2=ReadBody(fd) )  <0 || 
			( rval3=ReadTail(fd) )  <0 ? -1 : rval1+rval2+rval3);
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
LogRecord::WriteHeader(int fd)
{
	char op[20];
	int  len = sprintf(op, "%d ", op_type);
	return( write(fd, op, len) < len ? -1 : len );
}


int
LogRecord::WriteHeader(FILE *fp)
{
	char op[20];
	int  len = sprintf(op, "%d ", op_type);
	return( fprintf(fp, "%s ", op) < len ? -1 : len );
}


int
LogRecord::WriteTail(int fd)
{
	return( write(fd, "\n", 1) < 1 ? -1 : 1 );
}


int
LogRecord::WriteTail(FILE *fp)
{
	return( fprintf(fp, "\n") < 1 ? -1 : 1 );
}


int
LogRecord::ReadHeader(int fd)
{
	int	rval;
	char *op = NULL;

	rval = readword(fd, op);
	if (rval < 0) {
		return rval;
	}
	op_type = atoi(op);
	free(op);
	return rval;
}


int
LogRecord::ReadHeader(FILE *fp)
{
	return( fscanf(fp, "%d ", &op_type) < 1 ? -1 : 1 );
}


int
LogRecord::ReadTail(FILE *fp)
{
	return( 0 );
}


int
LogRecord::ReadTail(int fd)
{
	return( 0 );
}


LogRecord *
ReadLogEntry(int fd, LogRecord* (*InstantiateLogEntry)(int fd, int type))
{
	LogRecord		*log_rec;
	LogRecord		head_only;
	int				rval;

	rval = head_only.ReadHeader(fd);
	if (rval < 0) {
		return 0;
	}
	log_rec = InstantiateLogEntry(fd, head_only.get_op_type());
	if (head_only.ReadTail(fd) < 0) {
		delete log_rec;
		return 0;
	}
	return log_rec;
}


LogRecord *
ReadLogEntry(FILE *fp, LogRecord* (*InstantiateLogEntry)(FILE *fp, int type))
{
	LogRecord		*log_rec;
	LogRecord		head_only;
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
