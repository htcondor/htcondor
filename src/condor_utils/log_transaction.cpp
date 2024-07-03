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

Transaction::Transaction():  m_triggers(0) , m_EmptyTransaction(true)
{
}

Transaction::~Transaction()
{
	YourString key;

	for (const auto &[key, l]: op_log) {
		for (auto *log: *l) {
			delete log;
		}
		delete l;
	}
		// NOTE: the string_view keys in this hash table now contain
		// pointers to deallocated memory, as do the LogRecordList pointers.
		// No further lookups in this hash table should be performed.
}

void
Transaction::Commit(FILE* fp, const char *filename, LoggableClassAdTable *data_structure, bool nondurable)
{
	int fd;

	if ( filename == nullptr ) {
		filename = "<null>";
	}

		// We're seeing sporadic test suite failures where 
		// CommitTransaction() appears to take a long time to
		// execute. Timing the I/O operations will help us
		// narrow down the cause
	time_t before, after;

	for( auto *log: ordered_op_log) {
		if ( fp != nullptr ) {
			if ( log->Write( fp ) < 0 ) {
				EXCEPT( "write to %s failed, errno = %d", filename, errno );
			}
		}
		log->Play(data_structure);
	}

	if ( !nondurable && fp != nullptr ) {

		before = time(nullptr);
		if ( fflush( fp ) != 0 ){
			EXCEPT( "flush to %s failed, errno = %d", filename, errno );
		}
		after = time(nullptr);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG, "Transaction::Commit(): fflush() took %ld seconds to run\n", after - before );
		}

		before = time(nullptr);
		fd = fileno( fp );
		if ( fd >= 0 ) {
			if ( condor_fdatasync( fd ) < 0 ) {
				EXCEPT( "fdatasync of %s failed, errno = %d", filename, errno );
			}
		}
		after = time(nullptr);
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
	std::string_view key_obj = key ? key : "";

	LogRecordList *l = nullptr;
	auto it = op_log.find(key_obj);
	if (it == op_log.end()) {
		l = new LogRecordList;
		op_log.emplace(key_obj,l);
	} else {
		l = it->second;
	}
	l->emplace_back(log);
	ordered_op_log.emplace_back(log);
}

LogRecord *
Transaction::FirstEntry(char const *key)
{
	std::string_view key_obj = key;
	
	auto it = op_log.find(key_obj);
	if (it == op_log.end()) {
		return nullptr;
	}
	LogRecordList *lrl = it->second;

	op_log_iterating = lrl->begin();
	op_log_iterating_end = lrl->end();
	return *op_log_iterating++;
}

LogRecord *
Transaction::NextEntry()
{
	if (op_log_iterating == op_log_iterating_end) {
		return nullptr;
	}
	return *op_log_iterating++;
}

void
Transaction::InTransactionListKeysWithOpType( int op_type, std::list<std::string> &new_keys )
{
	for( auto *log: ordered_op_log) {
		if( log->get_op_type() == op_type ) {
			new_keys.emplace_back(log->get_key() );
		}
	}
}

bool Transaction::KeysInTransaction(std::set<std::string> & keys, bool add_keys /*=false*/)
{
	if ( ! add_keys) keys.clear();
	if (m_EmptyTransaction) return false;
	bool items_added = false;
	for (const auto &[key, l]: op_log) {
		if ( ! key.empty()) {
			items_added = true;
			keys.emplace(key);
		}
	}
	return items_added;
}

