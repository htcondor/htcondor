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
#include "stl_string_utils.h"

bool valid_record_optype(int optype) {
    switch (optype) {
        case CondorLogOp_NewClassAd:
        case CondorLogOp_DestroyClassAd:
        case CondorLogOp_SetAttribute:
        case CondorLogOp_DeleteAttribute:
        case CondorLogOp_BeginTransaction:
        case CondorLogOp_EndTransaction:
        case CondorLogOp_LogHistoricalSequenceNumber:
            return true;
        default:
            return false;
    }
    return false;
}


int
LogRecord::readword(FILE *fp, char * &str)
{
	int		i, bufsize = 1024;
	char	*buf = (char *)malloc(bufsize);
	int ch;

	if ( ! buf)
		return -1;

	// ignore leading whitespace but don't pass newline
	do {
		ch = fgetc( fp );
		if( ch == EOF || ch == '\0' ) {
			free( buf );
			return( -1 );
		}
		buf[0] = ch;
	} while (isspace((unsigned char)buf[0]) && buf[0]!='\n' );

	// read until whitespace
	for (i = 1; !isspace((unsigned char)buf[i-1]); i++) {
		if (i == bufsize) {
			void * vbuf = realloc(buf, bufsize*2);
			if ( ! vbuf) {
				free(buf);
				return -1;
			}
			buf = (char *)vbuf;
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

	if ( ! buf)
		return -1;

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
			void * vbuf = realloc(buf, bufsize*2);
			if ( ! vbuf) {
				free(buf);
				return -1;
			}
			buf = (char *)vbuf;
			bufsize *= 2;
		} 
		ch = fgetc( fp );
		if( ch == EOF || ch == '\0' ) {
			free( buf );
			return( -1 );
		}
		buf[i] = ch;
	}

		// treat no input as error
	if( i==1 ) {
		free( buf );
		return( -1 );
	}

	buf[i-1] = '\0';
	str = strdup((char *)buf);
	free(buf);
	return i-1;
}

// static buffer for readline_fast
char *LogRecord::linebuf{nullptr};
size_t LogRecord::bufsize{0};

/* static */ void LogRecord::free_linebuf()
{
	if (linebuf) {
		free(linebuf);
		linebuf = nullptr;
		bufsize = 0;
	}
}

/* static */ void LogRecord::reserve_linebuf(size_t cb)
{
	if (linebuf && cb > bufsize) {
		// if realloc succeed, our buffer was copied and freed.
		char * tmp = (char*)realloc(linebuf, cb+1);
		if (tmp) {
			bufsize = cb;
			linebuf = tmp;
		}
	}
	if ( ! linebuf) {
		bufsize = cb;
		linebuf = (char*)malloc(bufsize+1);
		if (linebuf) { memset(linebuf,0,bufsize+1); }
	}
}

/* static */ size_t LogRecord::linebuf_size()
{
	return bufsize;
}

/* static */ std::string_view LogRecord::readline_fast(FILE* fp, bool partial_line_ok /*=false*/)
{
	if ( ! fp || feof(fp)) return {};

	if ( ! linebuf) { reserve_linebuf(1024); }
	ASSERT(linebuf);

	char * end_ptr = linebuf;  // Pointer to read into next read

	for (;;) {
		size_t len = bufsize - (end_ptr - linebuf) + 1;
		if (len <= 5) {
			size_t end_off = end_ptr - linebuf; // remember our end position
			size_t old_size = bufsize;
			reserve_linebuf(bufsize*2); // grow buffer by doubling it
			if (old_size == bufsize) {
				// failed to grow the buffer
				EXCEPT( "Out of memory - classad log record too long" );
			} else {
				// grow successful, adjust to a new linebuf pointer
				end_ptr = linebuf + end_off;
				len = bufsize - end_off + 1;
			}
		}

		if ( ! fgets(end_ptr,len,fp)) {
			// we hit EOF (or error)
			if (*linebuf == 0) {
				return {}; // we read nothing at all
			} else {
				// if we got a newline in the buffer, then this is a good read
				// otherwise it is an error or partial line
				if (*end_ptr) {
					size_t cch = strlen(end_ptr);
					if (end_ptr[cch-1] == '\n') {
						end_ptr += cch-1;
						if (end_ptr > linebuf && end_ptr[-1] == '\r') {
							--end_ptr;
						}
						*end_ptr = 0;
						return {linebuf, end_ptr};
					}
				}
				if (partial_line_ok) {
					return {linebuf}; // return what we got
				}
				return {}; // no newline, so we read nothing
			}
		}

		// See if fgets read an entire line, or simply ran out of buffer space
		if (*end_ptr == 0) {
			continue;
		}

		size_t cch = strlen(end_ptr);
		if (end_ptr[cch-1] != '\n') {
			// if we made it here, fgets() ran out of buffer space.
			// move our read_ptr pointer forward so we concatenate the
			// rest on after we realloc our buffer above.
			end_ptr += cch;
			continue;	// since we are not finished reading this line
		}

		end_ptr += cch-1;
		if (end_ptr > linebuf && end_ptr[-1] == '\r') {
			--end_ptr;
		}
		*end_ptr = 0;
		return {linebuf, end_ptr};
	}
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
LogRecord::WriteHeader(FILE *fp) const
{
	char op[20];
	int  len = snprintf(op, sizeof(op), "%d ", op_type);
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

    op_type = CondorLogOp_Error;

    rval = readword(fp, op);
    if (rval < 0) {
        return rval;
    }
    YourStringDeserializer lex(op);
    if (!lex.deserialize_int(&op_type) || !valid_record_optype(op_type)) {
        op_type = CondorLogOp_Error;
    }
    free(op);
    return (op_type == CondorLogOp_Error) ? -1 : rval;
}


// The ReadBody() function in all of our child classes consume the newline
// at the end of every line, so we have nothing to read.
int
LogRecord::ReadTail(FILE *  /*fp*/)
{
	return( 0 );
}

LogRecord *
ReadLogEntry(FILE *fp, unsigned long recnum, LogRecord* (*InstantiateLogEntry)(FILE *fp, unsigned long recnum, int type, const ConstructLogEntry & ctor), const ConstructLogEntry & ctor)
{
    char* opword = NULL;
    int opcode = CondorLogOp_Error;
	int rval = LogRecord::readword(fp, opword);
	if (rval < 0) return NULL;
    YourStringDeserializer lex(opword);
    if (!lex.deserialize_int(&opcode) || !valid_record_optype(opcode)) {
        opcode = CondorLogOp_Error;
    }
    free(opword);

	return InstantiateLogEntry(fp, recnum, opcode, ctor);
}
