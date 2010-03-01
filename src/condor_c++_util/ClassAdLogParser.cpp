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
#include <stdlib.h> // for free, atoi, malloc, realloc
#include <string.h> // for strcpy, strdup
#include <ctype.h> // for isspace
#include <assert.h> // for assert
#include <errno.h> // for errno
#include <syslog.h> // for syslog, LOG_ERR
#else
#include "condor_common.h"
#include "condor_io.h"
#endif

#include "ClassAdLogEntry.h"
#include "ClassAdLogParser.h"

/***** Prevent calling free multiple times in this code *****/
/* This fixes bugs where we would segfault when reading in
 * a corrupted log file, because memory would be deallocated
 * both in ReadBody and in the destructor. 
 * To fix this, we make certain all calls to free() in this
 * file reset the pointer to NULL so we know now to call
 * it again. */
#ifdef free
#undef free
#endif
#define free(ptr) \
if (ptr) free(ptr); \
ptr = NULL;


//**************************************************************************
// constructor & destructor
//**************************************************************************

ClassAdLogParser::ClassAdLogParser()
{
	log_fp = NULL;
	nextOffset = 0;
}

ClassAdLogParser::~ClassAdLogParser()
{
	log_fp = NULL;
	nextOffset = 0;
}


int
ClassAdLogParser::getFileDescriptor()
{
	return fileno(log_fp);
}

void
ClassAdLogParser::setFileDescriptor(int fd)
{
    log_fp = fdopen(fd,"r");
}

FILE *
ClassAdLogParser::getFilePointer() 
{
	return log_fp;
}

void
ClassAdLogParser::setFilePointer(FILE *fp)
{
	log_fp = fp;
}

ClassAdLogEntry*
ClassAdLogParser::getLastCALogEntry()
{
	return &lastCALogEntry;
}

ClassAdLogEntry*
ClassAdLogParser::getCurCALogEntry()
{
	return &curCALogEntry;
}

void
ClassAdLogParser::setNextOffset(long offset)
{
		//we first check to see whether the default value is being
	    //used or if the user has actually provided the offset.  
	if (offset == IMPOSSIBLE_OFFSET) {  
		nextOffset = curCALogEntry.next_offset;
	}
	else {
		nextOffset = offset;
	}
}

FileOpErrCode 
ClassAdLogParser::openFile() {
    // open a job_queue.log file
#ifdef _NO_CONDOR_
    log_fp = fopen(job_queue_name, "r");
#else
    log_fp = safe_fopen_wrapper(job_queue_name, "r");
#endif

    if (log_fp == NULL) {
        return FILE_OPEN_ERROR;
    }
	return FILE_OP_SUCCESS;
}

FileOpErrCode 
ClassAdLogParser::closeFile() {
	if (log_fp != NULL) {
	  fclose(log_fp);
	  log_fp = NULL;
	}
	return FILE_OP_SUCCESS;
}


void
ClassAdLogParser::setJobQueueName(const char* jqn)
{
	strcpy(job_queue_name, jqn);
}

char*
ClassAdLogParser::getJobQueueName()
{
	return job_queue_name;
}

/*! read a classad log entry in the current offset of a job queue log file
 *
 * \return operation result status and command type
 */
FileOpErrCode
ClassAdLogParser::readLogEntry(int &op_type)
{
	int	rval;

    // move to the current offset
    if (fseek(log_fp, nextOffset, SEEK_SET) != 0) {
        fclose(log_fp);
        log_fp = NULL;
        return FILE_READ_EOF;
    }

    rval = readHeader(log_fp, op_type);
    if (rval < 0) {
        fclose(log_fp);
        log_fp = NULL;
        return FILE_READ_EOF;
    }

		// initialize of current & last ClassAd Log Entry objects
	lastCALogEntry.init(curCALogEntry.op_type);
	lastCALogEntry = curCALogEntry;
	curCALogEntry.init(op_type);
	curCALogEntry.offset = nextOffset;


		// read a ClassAd Log Entry Body
	switch(op_type) {
	    case CondorLogOp_LogHistoricalSequenceNumber:
            rval = readLogHistoricalSNBody(log_fp);
			break;
	    case CondorLogOp_NewClassAd:
            rval = readNewClassAdBody(log_fp);
			break;
	    case CondorLogOp_DestroyClassAd:
            rval = readDestroyClassAdBody(log_fp);
			break;
	    case CondorLogOp_SetAttribute:
            rval = readSetAttributeBody(log_fp);
			break;
	    case CondorLogOp_DeleteAttribute:
            rval = readDeleteAttributeBody(log_fp);
			break;
		case CondorLogOp_BeginTransaction:
            rval = readBeginTransactionBody(log_fp);
			break;
		case CondorLogOp_EndTransaction:
            rval = readEndTransactionBody(log_fp);
			break;
	    default:
            fclose(log_fp);
            log_fp = NULL;
		    return FILE_READ_ERROR;
			break;
	}

	if (rval < 0) {

			//
			// This code block is borrowed from log.C 
			//
		
			// check if this bogus record is in the midst of a transaction
			// (try to find a CloseTransaction log record)
		
		char	*line;

		int		op;

		if( !log_fp ) {
#ifdef _NO_CONDOR_
			syslog(LOG_ERR, "Failed fdopen() when recovering corrupt log file");
#else
			dprintf(D_ALWAYS, "Failed fdopen() when recovering corrupt log file");
#endif
			return FILE_FATAL_ERROR;
		}

		while( -1 != readline( log_fp, line ) ) {
			int rv = sscanf( line, "%d ", &op );
			free(line);
			if( rv != 1 ) {
					// no op field in line; more bad log records...
				continue;
			}
			if( op == CondorLogOp_EndTransaction ) {
					// aargh!  bad record in transaction.  abort!
#ifdef _NO_CONDOR_
				syslog(LOG_ERR, "Bad record with op=%d in corrupt logfile", op_type);
#else
				dprintf(D_ALWAYS, "Bad record with op=%d in corrupt logfile", op_type);
#endif
				return FILE_FATAL_ERROR;
			}
		}
		
		if( !feof( log_fp ) ) {
			fclose(log_fp);
            log_fp = NULL;
#ifdef _NO_CONDOR_
			syslog(LOG_ERR,
				   "Failed recovering from corrupt file, errno=%d (%m)",
				   errno);
#else
			dprintf(D_ALWAYS,
					"Failed recovering from corrupt file, errno=%d",
					errno);
#endif
			return FILE_FATAL_ERROR;
		}

			// there wasn't an error in reading the file, and the bad log 
			// record wasn't bracketed by a CloseTransaction; ignore all
			// records starting from the bad record to the end-of-file, and
		
			// pretend that we hit the end-of-file.
		fclose( log_fp );
        log_fp = NULL;

		curCALogEntry = lastCALogEntry;
		curCALogEntry.offset = nextOffset;

		return FILE_READ_EOF;
	}


	// get and set the new current offset
    nextOffset = ftell(log_fp);

	curCALogEntry.next_offset = nextOffset;

	return FILE_READ_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
ParserErrCode
ClassAdLogParser::getNewClassAdBody(char*& key, 
									char*& mytype, 
									char*& targettype)
{
	if (curCALogEntry.op_type != CondorLogOp_NewClassAd) {
		return PARSER_FAILURE;
	}
	key = strdup(curCALogEntry.key);
	mytype = strdup(curCALogEntry.mytype);
	targettype = strdup(curCALogEntry.targettype);
	
	return PARSER_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
ParserErrCode
ClassAdLogParser::getDestroyClassAdBody(char*& key)
{
	if (curCALogEntry.op_type != CondorLogOp_DestroyClassAd) {
		return PARSER_FAILURE;
	}

	key = strdup(curCALogEntry.key);

	return PARSER_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
ParserErrCode
ClassAdLogParser::getSetAttributeBody(char*& key, char*& name, char*& value)
{
	if (curCALogEntry.op_type != CondorLogOp_SetAttribute) {
		return PARSER_FAILURE;
	}

	key = strdup(curCALogEntry.key);
	name = strdup(curCALogEntry.name);
	value = strdup(curCALogEntry.value);

	return PARSER_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
ParserErrCode
ClassAdLogParser::getDeleteAttributeBody(char*& key, char*& name)
{
	if (curCALogEntry.op_type != CondorLogOp_DeleteAttribute) {
		return PARSER_FAILURE;
	}

	key = strdup(curCALogEntry.key);
	name = strdup(curCALogEntry.name);

	return PARSER_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
ParserErrCode
ClassAdLogParser::getLogHistoricalSNBody(char*& seqnum, char*& timestamp)
{
	if (curCALogEntry.op_type != CondorLogOp_LogHistoricalSequenceNumber) {
		return PARSER_FAILURE;
	}

	seqnum = strdup(curCALogEntry.key);
	timestamp = strdup(curCALogEntry.value);

	return PARSER_SUCCESS;
}

int
ClassAdLogParser::readNewClassAdBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_NewClassAd);

		// This code part is borrowed from LogNewClassAd::ReadBody 
		// in classad_log.C
	int rval, rval1;
	rval = readword(fp, curCALogEntry.key);
	if (rval < 0) {
		return rval;
	}
	rval1 = readword(fp, curCALogEntry.mytype);
	if (rval1 < 0) {
		return rval1;
	}
	rval += rval1;
	rval1 = readword(fp, curCALogEntry.targettype);
	if (rval1 < 0) {
		return rval1;
	}
	return rval + rval1;
}

int
ClassAdLogParser::readDestroyClassAdBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_DestroyClassAd);

		// This code part is borrowed from LogDestroyClassAd::ReadBody 
		// in classad_log.C
	return readword(fp, curCALogEntry.key);
}

int
ClassAdLogParser::readSetAttributeBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_SetAttribute);

		// This code part is borrowed from LogSetAttribute::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fp, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fp, curCALogEntry.name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	rval = readline(fp, curCALogEntry.value);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
}

int
ClassAdLogParser::readDeleteAttributeBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_DeleteAttribute);

		// This code part is borrowed from LogNewClassAd::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fp, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fp, curCALogEntry.name);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
}

int
ClassAdLogParser::readBeginTransactionBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_BeginTransaction);

		// This code part is borrowed from LogBeginTransaction::ReadBody 
		// in classad_log.C
	int 	ch;

	ch = fgetc(fp);

	if( ch == EOF || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}

int
ClassAdLogParser::readEndTransactionBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_EndTransaction);

		// This code part is borrowed from LogEndTransaction::ReadBody 
		// in classad_log.C
	int 	ch;

	ch = fgetc(fp);

	if( ch == EOF || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}

int
ClassAdLogParser::readLogHistoricalSNBody(FILE *fp)
{
	curCALogEntry.init(CondorLogOp_LogHistoricalSequenceNumber);

		// This code part is borrowed from LogSetAttribute::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fp, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fp, curCALogEntry.name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	rval = readline(fp, curCALogEntry.value);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
}


int
ClassAdLogParser::readHeader(FILE *fp, int& op_type)
{
	int	rval;
	char *op = NULL;

	rval = readword(fp, op);
	if (rval < 0) {
		return rval;
	}
	op_type = atoi(op);
	free(op);
	return rval;
}


int
ClassAdLogParser::readword(FILE *fp, char * &str)
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
			assert(buf);
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

	if(str != NULL) {
		free(str);
	}
	str = strdup(buf);
	free(buf);
	return i-1;
}



int
ClassAdLogParser::readline(FILE *fp, char * &str)
{
	int		i, bufsize = 4096;
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
			assert(buf);
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
