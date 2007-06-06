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

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "log_transaction.h"
#include "condor_debug.h"

#define TRANSACTION_HASH_LEN 10000

Transaction::Transaction(): op_log(TRANSACTION_HASH_LEN,YourSensitiveString::hashFunction,rejectDuplicateKeys)
{
	m_EmptyTransaction = true;
	op_log_iterating = NULL;
}

Transaction::~Transaction()
{
	LogRecordList *l;
	LogRecord		*log;
	YourSensitiveString key;

	op_log.startIterations();
	while( op_log.iterate(key,l) ) {
		ASSERT( l );
		l->Rewind();
		while( (log = l->Next()) ) {
			delete log;
		}
		delete l;
	}
		// NOTE: the YourSensitiveString keys in this hash table now contain
		// pointers to deallocated memory, as do the LogRecordList pointers.
		// No further lookups in this hash table should be performed.
}

void
Transaction::Commit(FILE* fp, void *data_structure, bool nondurable)
{
	LogRecord *log;
	int fd;

	ordered_op_log.Rewind();
	while( (log = ordered_op_log.Next()) ) {
		if (fp != NULL) {
			if (log->Write(fp) < 0) {
				EXCEPT("write inside a transaction failed, errno = %d",errno);
			}
		}
		log->Play(data_structure);
	}

	if( !nondurable ) {
		if (fp != NULL){
			if (fflush(fp) !=0){
				EXCEPT("fflush inside a transaction failed, errno = %d",errno);
			}
			fd = fileno(fp);
			if (fd >= 0) {
				if (fsync(fd) < 0) {
					EXCEPT("fsync inside a transaction failed, errno = %d",errno);
				}
			}
		}
	}
}

void
Transaction::AppendLog(LogRecord *log)
{
	m_EmptyTransaction = false;
	char const *key = log->get_key();
	YourSensitiveString key_obj = key ? key : "";

	LogRecordList *l = NULL;
	op_log.lookup(key_obj,l);
	if( !l ) {
		l = new LogRecordList;
		op_log.insert(key_obj,l);
	}
	l->Append(log);
	ordered_op_log.Append(log);
}

LogRecord *
Transaction::FirstEntry(char const *key)
{
	YourSensitiveString key_obj = key;
	op_log_iterating = NULL;
	op_log.lookup(key_obj,op_log_iterating);

	if( !op_log_iterating ) {
		return NULL;
	}

	op_log_iterating->Rewind();
	return op_log_iterating->Next();
}
LogRecord *
Transaction::NextEntry()
{
	ASSERT( op_log_iterating );
	return op_log_iterating->Next();
}
