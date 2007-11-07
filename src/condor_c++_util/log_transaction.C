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
