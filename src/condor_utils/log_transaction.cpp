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


#include "condor_common.h"
#include "log_transaction.h"
#include "condor_debug.h"
#include "condor_fsync.h"

Transaction::Transaction()
	: op_log(hashFunction)
	, op_log_iterating(NULL)
	, m_triggers(0)
	, m_EmptyTransaction(true)
{
}

Transaction::~Transaction()
{
	LogRecordList *l;
	LogRecord		*log;
	YourString key;

	op_log.startIterations();
	while( op_log.iterate(key,l) ) {
		ASSERT( l );
		l->Rewind();
		while( (log = l->Next()) ) {
			delete log;
		}
		delete l;
	}
		// NOTE: the YourString keys in this hash table now contain
		// pointers to deallocated memory, as do the LogRecordList pointers.
		// No further lookups in this hash table should be performed.
}

void
Transaction::Commit(FILE* fp, const char *filename, LoggableClassAdTable *data_structure, bool nondurable)
{
	LogRecord *log;
	int fd;

	if ( filename == NULL ) {
		filename = "<null>";
	}

	ordered_op_log.Rewind();

		// We're seeing sporadic test suite failures where 
		// CommitTransaction() appears to take a long time to
		// execute. Timing the I/O operations will help us
		// narrow down the cause
	time_t before, after;

	while( (log = ordered_op_log.Next()) ) {
		if ( fp != NULL ) {
			if ( log->Write( fp ) < 0 ) {
				EXCEPT( "write to %s failed, errno = %d", filename, errno );
			}
		}
		log->Play(data_structure);
	}

	if ( !nondurable && fp != NULL ) {

		before = time(NULL);
		if ( fflush( fp ) != 0 ){
			EXCEPT( "flush to %s failed, errno = %d", filename, errno );
		}
		after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG, "Transaction::Commit(): fflush() took %ld seconds to run\n", after - before );
		}

		before = time(NULL);
		fd = fileno( fp );
		if ( fd >= 0 ) {
			if ( condor_fdatasync( fd ) < 0 ) {
				EXCEPT( "fdatasync of %s failed, errno = %d", filename, errno );
			}
		}
		after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG, "Transaction::Commit(): fdatasync() took %ld seconds to run\n", after - before );
		}

	}
}

void
Transaction::AppendLog(LogRecord *log)
{
	m_EmptyTransaction = false;
	char const *key = log->get_key();
	YourString key_obj = key ? key : "";

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
	YourString key_obj = key;
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

void
Transaction::InTransactionListKeysWithOpType( int op_type, std::list<std::string> &new_keys )
{
	LogRecord *log;

	ordered_op_log.Rewind();
	while( (log = ordered_op_log.Next()) ) {
		if( log->get_op_type() == op_type ) {
			new_keys.push_back( log->get_key() );
		}
	}
}

bool Transaction::KeysInTransaction(std::set<std::string> & keys, bool add_keys /*=false*/)
{
	if ( ! add_keys) keys.clear();
	if (m_EmptyTransaction) return false;
	bool items_added = false;
	const YourString * key;
	LogRecordList ** dummy;
	op_log.startIterations();
	while(op_log.iterate_nocopy(&key, &dummy)) {
		if ( ! key->empty()) {
			items_added = true;
			keys.insert(key->c_str());
		}
	}
	return items_added;
}

