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
	int rval;

	rval = WriteHeader(fd);
	rval += WriteBody(fd);
	rval += WriteTail(fd);

	return rval;
}


int
LogRecord::Write(FILE *fp)
{
	return WriteHeader(fp) + WriteBody(fp) + WriteTail(fp);
}


int
LogRecord::Read(int fd)
{
	return ReadHeader(fd) + ReadBody(fd) + ReadTail(fd);
}


int
LogRecord::Read(FILE *fp)
{
	return ReadHeader(fp) + ReadBody(fp) + ReadTail(fp);
}


int
LogRecord::WriteHeader(int fd)
{
	char op[20];

	sprintf(op, "%d ", op_type);

	return write(fd, op, strlen(op));
}


int
LogRecord::WriteHeader(FILE *fp)
{
	return fprintf(fp, "%d ", op_type);
}


int
LogRecord::WriteTail(int fd)
{
	return write(fd, "\n", 1);
}


int
LogRecord::WriteTail(FILE *fp)
{
	return fprintf(fp, "\n");
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
	return fscanf(fp, "%d ", &op_type);
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
