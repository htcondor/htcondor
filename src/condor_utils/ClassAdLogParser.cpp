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
const char *EMPTY_CLASSAD_TYPE_NAME = "(empty)"; // normally in classad_log.h/cpp
#define ASSERT(x) assert(x)
#else
#include "condor_common.h"
#include "condor_io.h"
extern const char *EMPTY_CLASSAD_TYPE_NAME; // defined in classad_log.cpp
#endif

#include "ClassAdLogEntry.h"
#include "ClassAdLogParser.h"
#include "log.h"

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
	m_close_fp = true;
	nextOffset = 0;
	job_queue_name[0] = '\0';
}

ClassAdLogParser::~ClassAdLogParser()
{
	closeFile();
}


FILE *
ClassAdLogParser::getFilePointer() 
{
	return log_fp;
}

void
ClassAdLogParser::setFilePointer(FILE *fp)
{
	closeFile();
	log_fp = fp;
	m_close_fp = false;
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
	closeFile();
#ifdef _NO_CONDOR_
    log_fp = fopen(job_queue_name, "r");
#else
    log_fp = safe_fopen_wrapper_follow(job_queue_name, "r");
#endif

    if (log_fp == NULL) {
        return FILE_OPEN_ERROR;
    }
	m_close_fp = true;
	return FILE_OP_SUCCESS;
}

FileOpErrCode 
ClassAdLogParser::closeFile() {
	if (log_fp != NULL && m_close_fp) {
		fclose(log_fp);
	}
	log_fp = NULL;
	return FILE_OP_SUCCESS;
}


void
ClassAdLogParser::setJobQueueName(const char* jqn)
{
	size_t cch = strlen(jqn);
	ASSERT (cch < COUNTOF(job_queue_name));
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
    if (log_fp && fseek(log_fp, nextOffset, SEEK_SET) != 0) {
        closeFile();
        return FILE_READ_EOF;
    }

    if(log_fp) {
	    rval = readHeader(log_fp, op_type);
	    if (rval < 0) {
		    closeFile();
		    return FILE_READ_EOF;
	    }
    }

		// initialize of current & last ClassAd Log Entry objects
	lastCALogEntry.init(curCALogEntry.op_type);
	lastCALogEntry = curCALogEntry;
	curCALogEntry.init(op_type);
	curCALogEntry.offset = nextOffset;


		// read a ClassAd Log Entry Body
	if(log_fp) {
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
		    closeFile();
			    return FILE_READ_ERROR;
				break;
		}
	} else return FILE_READ_ERROR;

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
			dprintf(D_ALWAYS, "Failed fdopen() when recovering corrupt log file\n");
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
				dprintf(D_ALWAYS, "Bad record with op=%d in corrupt logfile\n", op_type);
#endif
				return FILE_FATAL_ERROR;
			}
		}
		
		if( !feof( log_fp ) ) {
			closeFile();
#ifdef _NO_CONDOR_
			syslog(LOG_ERR,
				   "Failed recovering from corrupt file, errno=%d (%m)",
				   errno);
#else
			dprintf(D_ALWAYS,
					"Failed recovering from corrupt file, errno=%d\n",
					errno);
#endif
			return FILE_FATAL_ERROR;
		}

			// there wasn't an error in reading the file, and the bad log 
			// record wasn't bracketed by a CloseTransaction; ignore all
			// records starting from the bad record to the end-of-file, and
		
			// pretend that we hit the end-of-file.
		closeFile();

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
									char*& targettype) const
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
ClassAdLogParser::getDestroyClassAdBody(char*& key) const
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
ClassAdLogParser::getSetAttributeBody(char*& key, char*& name, char*& value) const
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
ClassAdLogParser::getDeleteAttributeBody(char*& key, char*& name) const
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
ClassAdLogParser::getLogHistoricalSNBody(char*& seqnum, char*& timestamp) const
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
	if( curCALogEntry.mytype && strcmp(curCALogEntry.mytype,EMPTY_CLASSAD_TYPE_NAME)==0 ) {
		free(curCALogEntry.mytype);
		curCALogEntry.mytype = strdup("");
		ASSERT( curCALogEntry.mytype );
	}
	if (rval1 < 0) {
		return rval1;
	}
	rval += rval1;
	rval1 = readword(fp, curCALogEntry.targettype);
	if( curCALogEntry.targettype && strcmp(curCALogEntry.targettype,EMPTY_CLASSAD_TYPE_NAME)==0 ) {
		free(curCALogEntry.targettype);
		curCALogEntry.targettype = strdup("");
		ASSERT( curCALogEntry.targettype );
	}
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

	if( ch == EOF || (ch != '\n' && ch != '#')) {
		return( -1 );
	}
	// if the next character is a #, the remaineder of the line is a comment
	if (ch == '#') {
		readline(fp, curCALogEntry.value);
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
	return LogRecord::readword(fp,str);
}

int
ClassAdLogParser::readline(FILE *fp, char * &str)
{
	return LogRecord::readline(fp,str);
}
