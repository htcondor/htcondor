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

#if !defined(_LOG_TRANSACTION_H)
#define _LOG_TRANSACTION_H

/*
   This defines a transaction log for data structure operations.
   Calling Commit() will log the operations to disk and perform
   the operations on the data structure in memory.  Destroying 
   the Transaction class aborts the transaction.  Users are
   encouraged to log a BeginTransaction record before calling
   Commit() and an EndTransaction record after calling Commit()
   to handle crashes in the middle of a commit.  This interface
   does not include BeginTransaction and EndTransaction records
   so they can be defined as deemed appropriate by the user.
*/

#include <list>
#include <string>
#include <set>
#include <map>
#include <string_view>
#include "log.h"

using LogRecordList = std::vector<LogRecord *>;

class LoggableClassAdTable;

class Transaction {
public:
	Transaction();
	~Transaction();
	void Commit(FILE* fp, const char *filename, LoggableClassAdTable *data_structure, bool nondurable=false);
	void AppendLog(LogRecord *);
	LogRecord *FirstEntry(char const *key);
	LogRecord *NextEntry();
	bool EmptyTransaction() const { return m_EmptyTransaction; }
	void InTransactionListKeysWithOpType( int op_type, std::list<std::string> &new_keys );
	bool KeysInTransaction(std::set<std::string> & keys, bool add_keys=false);
	// transaction triggers are used by clients of this class to keep track of things that the client needs to do on commit
	// they are stored here, but only the client knows what they mean.
	int  SetTriggers(int mask) { m_triggers |= mask; return m_triggers; }
	int  GetTriggers() const { return m_triggers; }
private:
	std::map<std::string_view,LogRecordList *> op_log;
	LogRecordList ordered_op_log;
	LogRecordList::iterator op_log_iterating;
	LogRecordList::iterator op_log_iterating_end;
	int  m_triggers; // for use by transaction users to record transaction triggers.
	bool m_EmptyTransaction;
};

#endif
