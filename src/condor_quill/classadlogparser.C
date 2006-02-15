/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_io.h"

#include "classadlogentry.h"
#include "classadlogparser.h"

//! Definition of Maximum Length of Attribute List Expression
#ifndef ATTRLIST_MAX_EXPRESSION
#define ATTRLIST_MAX_EXPRESSION 	10240
#endif

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
	log_fd = -1;
	nextOffset = 0;
}

ClassAdLogParser::~ClassAdLogParser()
{
	log_fp = NULL;
	log_fd = -1;
	nextOffset = 0;
}


int
ClassAdLogParser::getFileDescriptor()
{
	return log_fd;
}

void
ClassAdLogParser::setFileDescriptor(int fd)
{
	log_fd = fd;
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
ClassAdLogParser::openFile(bool ex) {
	if (ex == false) { // Standard File I/O Operations
			// open a job_queue.log file
		log_fd = open(job_queue_name, O_RDONLY);
		
		if (log_fd < 0) {
			dprintf(D_ALWAYS, "[QUILL] Error in Opening a job_queue.log file!\n");
			return FILE_OPEN_ERROR;
		}
	}
	else { // File Stream I/O Operations
		// open a job_queue.log file
		log_fp = fopen(job_queue_name, "r");

		if (log_fp == NULL) {
			dprintf(D_ALWAYS, "[QUILL] Error in Opening a job_queue.log file!\n");
			return FILE_OPEN_ERROR;
		}
	}
	return FILE_OP_SUCCESS;
}

FileOpErrCode 
ClassAdLogParser::closeFile(bool ex) {
	if (ex == false && log_fd != -1) {
	  close(log_fd);
	  log_fd = -1;
	}
	if (ex == true && log_fp != NULL) {
	  fclose(log_fp);
	  log_fp = NULL;
	}
	return FILE_OP_SUCCESS;
}


void
ClassAdLogParser::setJobQueueName(char* jqn)
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
 * \param ex determine if the file is through standard file I/O 
 * 			 or stream file I/O (default is false, which means 
 * 			 standard file I/O).
 * \return operation result status and command type
 */
FileOpErrCode
ClassAdLogParser::readLogEntry(int &op_type, bool ex)
{
	int	rval;

	if (ex == false) { // Standard File I/O Operations
		// move to the current offset
		if (lseek(log_fd, nextOffset, SEEK_SET) != nextOffset) {
			close(log_fd);
			log_fd = -1;
			return FILE_READ_EOF;
		}

		rval = readHeader(log_fd, op_type);
		if (rval < 0) {
		  close(log_fd);
		  log_fd = -1;
		  return FILE_READ_EOF;
		}

	}
	else { // File Stream I/O Operations
		// move to the current offset
		if (fseek(log_fp, nextOffset, SEEK_SET) != nextOffset) {
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
	}

		// initialize of current & last ClassAd Log Entry objects
	lastCALogEntry.init(curCALogEntry.op_type);
	lastCALogEntry = curCALogEntry;
	curCALogEntry.init(op_type);
	curCALogEntry.offset = nextOffset;


		// read a ClassAd Log Entry Body
	switch(op_type) {
	    case CondorLogOp_LogHistoricalSequenceNumber:
			if(ex == false) {
				rval = readLogHistoricalSNBody(log_fd);
			}
			else {
				rval = readLogHistoricalSNBody(log_fp);
			}
			break;
	    case CondorLogOp_NewClassAd:
			if (ex == false) {
				rval = readNewClassAdBody(log_fd);
			}
			else {
				rval = readNewClassAdBody(log_fp);
			}
			break;
	    case CondorLogOp_DestroyClassAd:
			if (ex == false) {
				rval = readDestroyClassAdBody(log_fd);
			}
			else {
				rval = readDestroyClassAdBody(log_fp);
			}
			break;
	    case CondorLogOp_SetAttribute:
			if (ex == false) {
				rval = readSetAttributeBody(log_fd);
			}
			else {
				rval = readSetAttributeBody(log_fp);
			}
			break;
	    case CondorLogOp_DeleteAttribute:
			if (ex == false) {
				rval = readDeleteAttributeBody(log_fd);
			}
			else {
				rval = readDeleteAttributeBody(log_fp);
			}
			break;
		case CondorLogOp_BeginTransaction:
			if (ex == false) {
				rval = readBeginTransactionBody(log_fd);
			}
			else {
				rval = readBeginTransactionBody(log_fp);
			}
			break;
		case CondorLogOp_EndTransaction:
			if (ex == false) {
				rval = readEndTransactionBody(log_fd);
			}
			else {
				rval = readEndTransactionBody(log_fp);
			}
			break;
	    default:
			if(ex == false) {
				close(log_fd);
				log_fd = -1;
			}
			else {
				fclose(log_fp);
				log_fp = NULL;
			}
		    return FILE_READ_ERROR;
			break;
	}

	if (rval < 0) {

			//
			// This code block is borrowed from log.C 
			//
		
			// check if this bogus record is in the midst of a transaction
			// (try to find a CloseTransaction log record)
		
		char	line[ATTRLIST_MAX_EXPRESSION + 64];
		FILE 	*fp;
		if (ex == false) {
			fp = fdopen(log_fd, "r" );
		}
		else {
		  fp = log_fp;
		}
		int		op;

		if( !fp ) {
			EXCEPT("Failed fdopen() when recovering corrupt log file");
		}

		while( fgets( line, ATTRLIST_MAX_EXPRESSION+64, fp ) ) {
			if( sscanf( line, "%d ", &op ) != 1 ) {
					// no op field in line; more bad log records...
				continue;
			}
			if( op == CondorLogOp_EndTransaction ) {
					// aargh!  bad record in transaction.  abort!
				EXCEPT("Bad record with op=%d in corrupt logfile",
					   op_type);
			}
		}
		
		if( !feof( fp ) ) {
			fclose(fp);
			EXCEPT("Failed recovering from corrupt file, errno=%d",
				   errno );
		}

			// there wasn't an error in reading the file, and the bad log 
			// record wasn't bracketed by a CloseTransaction; ignore all
			// records starting from the bad record to the end-of-file, and
		
			// pretend that we hit the end-of-file.
		fclose( fp );

		curCALogEntry = lastCALogEntry;
		curCALogEntry.offset = nextOffset;

		return FILE_READ_EOF;
	}


	// get and set the new current offset
	if (ex == false) {
		nextOffset = lseek(log_fd, 0, SEEK_CUR);
	}
	else {
		nextOffset = ftell(log_fp);
	}

	curCALogEntry.next_offset = nextOffset;

	return FILE_READ_SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
QuillErrCode
ClassAdLogParser::getNewClassAdBody(char*& key, 
									char*& mytype, 
									char*& targettype)
{
	if (curCALogEntry.op_type != CondorLogOp_NewClassAd) {
		return FAILURE;
	}
	key = strdup(curCALogEntry.key);
	mytype = strdup(curCALogEntry.mytype);
	targettype = strdup(curCALogEntry.targettype);
	
	return SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
QuillErrCode
ClassAdLogParser::getDestroyClassAdBody(char*& key)
{
	if (curCALogEntry.op_type != CondorLogOp_DestroyClassAd) {
		return FAILURE;
	}

	key = strdup(curCALogEntry.key);

	return SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
QuillErrCode
ClassAdLogParser::getSetAttributeBody(char*& key, char*& name, char*& value)
{
	if (curCALogEntry.op_type != CondorLogOp_SetAttribute) {
		return FAILURE;
	}

	key = strdup(curCALogEntry.key);
	name = strdup(curCALogEntry.name);
	value = strdup(curCALogEntry.value);

	return SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
QuillErrCode
ClassAdLogParser::getDeleteAttributeBody(char*& key, char*& name)
{
	if (curCALogEntry.op_type != CondorLogOp_DeleteAttribute) {
		return FAILURE;
	}

	key = strdup(curCALogEntry.key);
	name = strdup(curCALogEntry.name);

	return SUCCESS;
}

/*!
	\warning each pointer must be freed by a calling funtion
*/
QuillErrCode
ClassAdLogParser::getLogHistoricalSNBody(char*& seqnum, char*& timestamp)
{
	if (curCALogEntry.op_type != CondorLogOp_LogHistoricalSequenceNumber) {
		return FAILURE;
	}

	seqnum = strdup(curCALogEntry.key);
	timestamp = strdup(curCALogEntry.value);

	return SUCCESS;
}

int
ClassAdLogParser::readNewClassAdBody(int fd)
{
	curCALogEntry.init(CondorLogOp_NewClassAd);

		// This code part is borrowed from LogNewClassAd::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval = readword(fd, curCALogEntry.key);
	if (rval < 0) {
		return rval;
	}

	rval1 = readword(fd, curCALogEntry.mytype);
	if (rval1 < 0) {
		return rval;
	}

	rval += rval1;

	rval1 = readword(fd, curCALogEntry.targettype);
	if (rval1 < 0) {
		return rval;
	}

	return rval + rval1;
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
ClassAdLogParser::readDestroyClassAdBody(int fd)
{
	curCALogEntry.init(CondorLogOp_DestroyClassAd);

		// This code part is borrowed from LogDestroyClassAd::ReadBody 
		// in classad_log.C
	return readword(fd, curCALogEntry.key);
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
ClassAdLogParser::readSetAttributeBody(int fd)
{
	curCALogEntry.init(CondorLogOp_SetAttribute);

		// This code part is borrowed from LogSetAttribute::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fd, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fd, curCALogEntry.name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	rval = readline(fd, curCALogEntry.value);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
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
ClassAdLogParser::readDeleteAttributeBody(int fd)
{
	curCALogEntry.init(CondorLogOp_DeleteAttribute);

		// This code part is borrowed from LogNewClassAd::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fd, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fd, curCALogEntry.name);
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
ClassAdLogParser::readBeginTransactionBody(int fd)
{
	curCALogEntry.init(CondorLogOp_BeginTransaction);

		// This code part is borrowed from LogBeginTransaction::ReadBody 
		// in classad_log.C
	char 	ch;
	int		rval = read( fd, &ch, 1 );
	if( rval < 1 || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
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
ClassAdLogParser::readEndTransactionBody(int fd)
{
	curCALogEntry.init(CondorLogOp_EndTransaction);

		// This code part is borrowed from LogEndTransaction::ReadBody 
		// in classad_log.C
	char 	ch;
	int		rval = read( fd, &ch, 1 );
	if( rval < 1 || ch != '\n' ) {
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
ClassAdLogParser::readLogHistoricalSNBody(int fd)
{
	curCALogEntry.init(CondorLogOp_LogHistoricalSequenceNumber);

		// This code part is borrowed from LogSetAttribute::ReadBody 
		// in classad_log.C
	int rval, rval1;

	rval1 = readword(fd, curCALogEntry.key);
	if (rval1 < 0) {
		return rval1;
	}

	rval = readword(fd, curCALogEntry.name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	rval = readline(fd, curCALogEntry.value);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
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
ClassAdLogParser::readHeader(int fd, int& op_type)
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
ClassAdLogParser::readword(int fd, char * &str)
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
			if(!buf) {
				EXCEPT("Call to realloc failed\n");
			}
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
			if(!buf) {
				EXCEPT("Call to realloc failed\n");
			}
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
ClassAdLogParser::readline(int fd, char * &str)
{
	int		i, rval, bufsize = 4096;
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
			if(!buf) {
				EXCEPT("Call to realloc failed\n");
			}
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
			if(!buf) {
				EXCEPT("Call to realloc failed\n");
			}
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
