/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#if !defined(_QMGMT_LOG_H)
#define _QMGMT_LOG_H

/*
   The ClassAdLog class is used throughout Condor for persistent storage
   of ClassAds.  ClassAds are stored in memory using the ClassAdHashTable
   class (see classad_hashtable.h) and stored on disk in ascii format 
   using the LogRecord class hierarchy (see log.h).  The Transaction
   class (see log_transaction.h) is used for simple transactions on
   the hash table.  Currently, only one transaction can be active.  When
   no transaction is active, the log operation is immediately written
   to disk and performed on the hash table.

   Log operations are not performed on the hash table until the transaction
   is committed.  To see updates to the table before they are committed,
   use the LookupInTransaction method.  To provide strict transaction
   semantics, an interface which uses the ClassAdLog should always call
   LookupInTransaction before looking up a value in the hash table when
   a transaction is active.

   The constructor will ignore any incomplete transactions written to the
   log.  The LogBeginTransaction and LogEndTransaction classes are used
   internally by ClassAdLog to delimit transactions in the on-disk log.
*/

#include "condor_classad.h"
#include "log.h"
#include "log_transaction.h"
#include "stopwatch.h"

extern const char *EMPTY_CLASSAD_TYPE_NAME;

// This class is used to abstract creation and destruction of 
// members of he ClassAdLog hashtable so that types derived from ClassAd
// but that that are not known to this header file can be used. 
template <typename AD>
class ConstructClassAdLogTableEntry : public ConstructLogEntry
{
public:
	virtual ClassAd* New(const char * /*key*/, const char * /*mytype*/) const { return new AD(); }
	virtual void Delete(ClassAd*& val) const { delete val; }
};

template <>
class ConstructClassAdLogTableEntry<ClassAd*> : public ConstructLogEntry
{
public:
	ConstructClassAdLogTableEntry() {}
	virtual ClassAd* New(const char * /*key*/, const char * /*mytype*/) const { return new ClassAd(); }
	virtual void Delete(ClassAd*& val) const { delete val; }
};

// declare a default ClassAdLog table entry maker for the normal case
// when the log holds ClassAd's and not some derived type.
#ifdef WIN32
// windows (or probably not-using-shared-libaries?) requires this to be declared here and not externed.
const ConstructClassAdLogTableEntry<ClassAd*> DefaultMakeClassAdLogTableEntry;
#else
// g++ (or using-shared-libaries?) requires this to be declared in a .cpp file and extern'ed here.
extern const ConstructClassAdLogTableEntry<ClassAd*> DefaultMakeClassAdLogTableEntry;
#endif

template <typename K, typename AD>
class ClassAdLog {
public:

	ClassAdLog(const ConstructLogEntry* pc=NULL);
	ClassAdLog(const char *filename,int max_historical_logs=0,const ConstructLogEntry* pc=NULL);
	~ClassAdLog();

	// define an stl type iterator, but one that can filter based on a requirements expression
	class filter_iterator : std::iterator<std::input_iterator_tag, AD> {
		private:
			HashTable<K,AD> *m_table;
			HashIterator<K,AD> m_cur;
			bool m_found_ad;
			const classad::ExprTree *m_requirements;
			int m_timeslice_ms;
			int m_done;
			int m_options;

		public:
			filter_iterator(ClassAdLog<K,AD> &log, const classad::ExprTree *requirements, int timeslice_ms, bool at_end=false)
				: m_table(&log.table)
				, m_cur(log.table.begin())
				, m_found_ad(false)
				, m_requirements(requirements)
				, m_timeslice_ms(timeslice_ms)
				, m_done(at_end)
				, m_options(0) {}

			~filter_iterator() {}
			AD operator *() const {
				if (m_done || (m_cur == m_table->end()) || !m_found_ad)
					return NULL;
				return (*m_cur).second;
			}
			AD operator ->() const;
			filter_iterator operator++();
			filter_iterator operator++(int);
			bool operator==(const filter_iterator &rhs) {
				if (m_table != rhs.m_table) return false;
				if (m_done && rhs.m_done) return true;
				if (m_done != rhs.m_done) return false;
				if (!(m_cur == rhs.m_cur) ) return false;
				return true;
			}
			bool operator!=(const filter_iterator &rhs) {return !(*this == rhs);}
			int set_options(int options) { int opts = m_options; m_options = options; return opts; }
			int get_options() { return m_options; }
	};


	void AppendLog(LogRecord *log);	// perform a log operation
	bool TruncLog();				// clean log file on disk

	void BeginTransaction();
	bool AbortTransaction();
	void CommitTransaction(const char * comment = NULL);
	void CommitNondurableTransaction(const char * comment = NULL);
	bool InTransaction() { return active_transaction != NULL; }
	int SetTransactionTriggers(int mask);
	int GetTransactionTriggers();

	/** Get a list of all new keys created in this transaction
		@param new_keys List object to populate
	*/
	void ListNewAdsInTransaction( std::list<std::string> &new_keys );

	/** Get the set of all keys mentioned in this transaction
	   returns false if there is not currently a transaction, true if there is.
	*/
	bool GetTransactionKeys( std::set<std::string> &keys );

		// increase non-durable commit level
		// if > 0, begin non-durable commits
		// return old level
	int IncNondurableCommitLevel();
		// decrease non-durable commit level and verify that it
		// matches old_level
		// if == 0, resume durable commits
	void DecNondurableCommitLevel(int old_level );

		// Flush the log output buffer (but do not fsync).
		// This is useful if non-durable events have been recently logged.
		// Flushing will allow other processes that read the log to see
		// the events that might otherwise hang around in the output buffer
		// for a long time.
	void FlushLog();

		// Force the log output buffer to non-volatile storage (disk).  
		// This means doing both a flush and fsync.
	void ForceLog();

	bool AdExistsInTableOrTransaction(const K& key);

	// returns 1 and sets val if corresponding SetAttribute found
	// returns 0 if no SetAttribute found
	// return -1 if DeleteAttribute or DestroyClassAd found
	int LookupInTransaction(const K& key, const char *name, char *&val);

	// insert into the given ad any attributes found in the uncommitted transaction
	// cache that match the key.  return true if any attributes were
	// added into the ad, false if not.
	bool AddAttrsFromTransaction(const K &key, ClassAd &ad);

	// insert into the given set any attribute names found in the uncommitted transaction
	// cache that match the key.  return true if any attributes were
	// added into the set, false if not.
	bool AddAttrNamesFromTransaction(const K &key, classad::References & attrs);

	HashTable<K,AD> table;

	// user-replacable helper class for creating and destroying values for the hashtable
	// this allows a user of the classad log to put a class derived from ClassAd in the hashtable
	// without the ClassAdLog code needing to know all about it.
	const ConstructLogEntry * make_table_entry;
	const ConstructLogEntry& GetTableEntryMaker() { if (make_table_entry) return *make_table_entry; return DefaultMakeClassAdLogTableEntry; }

	// When the log is truncated (i.e. compacted), old logs
	// may be saved, up to some limit.  The default is not
	// to save any history (max = 0).
	void SetMaxHistoricalLogs(int max) { this->max_historical_logs = max; }
	int GetMaxHistoricalLogs() { return max_historical_logs; }

	time_t GetOrigLogBirthdate() {return m_original_log_birthdate;}

protected:
	/** Returns handle to active transaction.  Upon return of this
		method, any active transaction is forgotten.  It is the caller's
		responsibility to eventually delete the handle returned.
		@return Pointer to the transaction state, or NULL if no transaction
		currently active.
		@see setActiveTransaction
	*/
	Transaction *getActiveTransaction();

	/** Sets the active transaction to the provided handle.  Will fail
		if there is currently a transaction already active.  Upon successful return,
		the callers handle will be reset to NULL, as the caller should no 
		longer touch the handle (including delete it).
		@param transaction A pointer to a transaction object previously
		returned by getActiveTransaction().
		@return True on success, else false.
		@see getActiveTransaction
	*/
	bool setActiveTransaction(Transaction* & transaction);

	int ExamineTransaction(const K& key, const char *name, char *&val, ClassAd* &ad);


private:
	void LogState(FILE* fp);
	FILE* log_fp;

	char const *logFilename() { return log_filename_buf.c_str(); }
	MyString log_filename_buf;
	Transaction *active_transaction;
	int max_historical_logs;
	unsigned long historical_sequence_number;
	time_t m_original_log_birthdate;
	int m_nondurable_level;

	bool SaveHistoricalLogs();
};


class LogHistoricalSequenceNumber : public LogRecord {
public:
	LogHistoricalSequenceNumber(unsigned long historical_sequence_number, time_t timestamp);
	int Play(void *data_structure);

	unsigned long get_historical_sequence_number() const {return historical_sequence_number;}
	time_t get_timestamp() const {return timestamp;}

private:
	virtual int WriteBody(FILE *fp);
	virtual int ReadBody(FILE *fp);

	virtual char const *get_key() {return NULL;}

	unsigned long historical_sequence_number;
	time_t timestamp; //when was the the first record originally written,
					  // regardless of how many times the log has rotated
};

// this class is the interface that is consumed by classes in this file that are derived from LogRecord
// The methods return true on success and false on failure.
// nextIteration() returns false when there are no more items.
class LoggableClassAdTable {
public:
	virtual ~LoggableClassAdTable() {};
	virtual bool lookup(const char * key, ClassAd*& ad) = 0;
	virtual bool remove(const char * key) = 0;
	virtual bool insert(const char * key, ClassAd* ad) = 0;
	virtual void startIterations() = 0; // begin iterations
	virtual bool nextIteration(const char*& key, ClassAd*&ad) = 0;
protected:
	LoggableClassAdTable() {};
};

// This is the implementation of the LoggableClassAdTable interface that makes the ClassAdLog's hashtable
// accessiable to the LogRecord classes without requiring them to know about the ClassAdLog.
// Instances of this class are constructed and passed to the Transaction methods and to the Play() method
// of the LogRecord classes.
template <typename K, typename AD>
class ClassAdLogTable : public LoggableClassAdTable {
public:
	ClassAdLogTable(HashTable<K,AD> & _table) : table(_table) {}
	virtual ~ClassAdLogTable() {};
	virtual bool lookup(const char * key, ClassAd*& out) {
		AD Ad = NULL;
		int iret = table.lookup(K(key), Ad);
		if( iret >= 0 ) {
			out = static_cast<ClassAd *>(Ad);
			return true;
		} else {
			return false;
		}
	}
	virtual bool remove(const char * key) { return table.remove(K(key)) >= 0; }
	virtual bool insert(const char * key, ClassAd * ad) { int iret = table.insert(K(key), AD(ad)); return iret >= 0; }
	virtual void startIterations() { table.startIterations(); } // begin iterations
	virtual bool nextIteration(const char*& key, ClassAd*&ad) {
		K k; AD Ad;
		int iret = table.iterate(k, Ad);
		if (iret != 1) {
			key = NULL;
			ad = NULL;
			return false;
		}
		current_key = k;
		key = current_key.c_str();
		ad = Ad;
		return true;
	}
protected:
	HashTable<K,AD> & table;
	std::string current_key; // used during iteration
};


class LogNewClassAd : public LogRecord {
public:
	LogNewClassAd(const char *key, const char *mytype, const char *targettype, const ConstructLogEntry & ctor_in = DefaultMakeClassAdLogTableEntry);
	virtual ~LogNewClassAd();
	int Play(void * data_structure); // data_structure should be of type LoggableClassAdTable *
	virtual char const *get_key() { return key; }
	char const *get_mytype() { return mytype; }
	char const *get_targettype() { return targettype; }

private:
	virtual int WriteBody(FILE *fp);
	virtual int ReadBody(FILE* fp);

	const ConstructLogEntry & ctor;
	char *key;
	char *mytype;
	char *targettype;
};


class LogDestroyClassAd : public LogRecord {
public:
	LogDestroyClassAd(const char *key, const ConstructLogEntry & ctor_in = DefaultMakeClassAdLogTableEntry);
	virtual ~LogDestroyClassAd();
	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
	virtual char const *get_key() { return key; }

private:
	virtual int WriteBody(FILE* fp) { size_t r=fwrite(key, sizeof(char), strlen(key), fp); return (r < strlen(key)) ? -1 : (int)r;}
	virtual int ReadBody(FILE* fp);

	const ConstructLogEntry & ctor;
	char *key;
};


class LogSetAttribute : public LogRecord {
public:
	LogSetAttribute(const char *key, const char *name, const char *value, const bool dirty=false);
	virtual ~LogSetAttribute();
	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
	virtual char const *get_key() { return key; }
	char const *get_name() { return name; }
	char const *get_value() { return value; }
    ExprTree* get_expr() { return value_expr; }

private:
	virtual int WriteBody(FILE* fp);
	virtual int ReadBody(FILE* fp);

	char *key;
	char *name;
	char *value;
	bool is_dirty;
    ExprTree* value_expr;    
};

class LogDeleteAttribute : public LogRecord {
public:
	LogDeleteAttribute(const char *key, const char *name);
	virtual ~LogDeleteAttribute();
	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
	virtual char const *get_key() { return key; }
	char const *get_name() { return name; }

private:
	virtual int WriteBody(FILE* fp);
	virtual int ReadBody(FILE* fp);

	char *key;
	char *name;
};

class LogBeginTransaction : public LogRecord {
public:
	LogBeginTransaction() { op_type = CondorLogOp_BeginTransaction; }
	virtual ~LogBeginTransaction(){};

	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
private:

	virtual int WriteBody(FILE* /*fp*/) {return 0;}
	virtual int ReadBody(FILE* fp);

	virtual char const *get_key() {return NULL;}
};

class LogEndTransaction : public LogRecord {
public:
	LogEndTransaction() : comment(NULL) { op_type = CondorLogOp_EndTransaction; }
	virtual ~LogEndTransaction() { free(comment); comment = NULL; }

	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
	const char * get_comment() { return comment; }
	void set_comment(const char * cmt) {
		// delete the old comment and set the new one
		// never set an empty comment, that would lead to a parse error on read-back
		if (comment) { free(comment); }
		if (cmt && cmt[0]) {
			comment = strdup(cmt);
		}
	}
private:
	virtual int WriteBody(FILE* fp);
	virtual int ReadBody(FILE* fp);

	virtual char const *get_key() {return NULL;}
	char * comment;
};

// These are non-templated helper functions that do most of the work of the classad log
// they make it possible for the ClassAdLog to work with types that are not known
// to this header file. (this works via the LoggableClassAdTable & ConstructLogEntry helper classes)
//
bool TruncateClassAdLog(
	const char * filename,          // in
	LoggableClassAdTable & la,      // in
	const ConstructLogEntry& maker, // in
	FILE* &log_fp,                  // in,out
	unsigned long & historical_sequence_number, // in,out
	time_t & m_original_log_birthdate, // in,out
	MyString & errmsg);             // out

bool WriteClassAdLogState(
	FILE *fp,                       // in
	const char * filename,          // in: used for error messages
	unsigned long sequence_number,  // in
	time_t original_log_birthdate,  // in
	LoggableClassAdTable & la,      // in
	const ConstructLogEntry& maker, // in
	MyString & errmsg);             // out

FILE* LoadClassAdLog(
	const char *filename,           // in
	LoggableClassAdTable & table,   // in
	const ConstructLogEntry& maker, // in
	unsigned long & historical_sequence_number, // in,out
	time_t & m_original_log_birthdate, // in,out
	bool & is_clean,  // out: true if log was shutdown cleanly
	bool & requires_successful_cleaning, // out: true if log must be cleaned (i.e rotated) before it can be written to again.
	MyString & errmsg);             // out, contains error or warning messages

int FlushClassAdLog(FILE* fp, bool force);

bool SaveHistoricalClassAdLogs(
	const char * filename,
	const unsigned long max_historical_logs,
	const unsigned long historical_sequence_number);

int ExamineLogTransaction(
	Transaction* active_transaction,
	const ConstructLogEntry& maker, // in
	const char * key,
	const char *name,
	char *&val,
	ClassAd* &ad);

bool AddAttrsFromLogTransaction(
	Transaction* active_transaction,
	const ConstructLogEntry& maker, // in
	const char * key,
	ClassAd &ad);

bool AddAttrNamesFromLogTransaction(
	Transaction* active_transaction,
	const char * key,
	classad::References & attrs); // out, attribute names are added when the transaction refers to them for the given key

LogRecord* InstantiateLogEntry(
	FILE* fp,
	unsigned long recnum,
	int type,
	const ConstructLogEntry & ctor);

// Templated member functions that call the helper functions with the correct arguments.
//

template <typename K, typename AD>
ClassAdLog<K,AD>::ClassAdLog(const char *filename,int max_historical_logs_arg,const ConstructLogEntry* maker)
	: table(hashFunction)
	, make_table_entry(maker)
{
	log_filename_buf = filename;
	active_transaction = NULL;
	m_nondurable_level = 0;

	bool open_read_only = max_historical_logs_arg < 0;
	if (open_read_only) { max_historical_logs_arg = -max_historical_logs_arg; }
	this->max_historical_logs = max_historical_logs_arg;

	bool is_clean = true;
	bool requires_successful_cleaning = false;
	MyString errmsg;

	ClassAdLogTable<K,AD> la(table); // this gives the ability to add & remove table items.

	log_fp = LoadClassAdLog(filename,
		la, this->GetTableEntryMaker(),
		historical_sequence_number, m_original_log_birthdate,
		is_clean, requires_successful_cleaning, errmsg);

	if ( ! log_fp) {
		EXCEPT("%s", errmsg.c_str());
	} else if ( ! errmsg.empty()) {
		dprintf(D_ALWAYS, "ClassAdLog %s has the following issues: %s\n", filename, errmsg.c_str());
	}
	if( !is_clean || requires_successful_cleaning ) {
		if (open_read_only && requires_successful_cleaning) {
			EXCEPT("Log %s is corrupt and needs to be cleaned before restarting HTCondor", filename);
		}
		else if( !TruncLog() && requires_successful_cleaning ) {
			EXCEPT("Failed to rotate ClassAd log %s.", filename);
		}
	}
}

template <typename K, typename AD>
ClassAdLog<K,AD>::ClassAdLog(const ConstructLogEntry* maker)
	: table(hashFunction)
	, make_table_entry(maker)
{
	active_transaction = NULL;
	log_fp = NULL;
	m_nondurable_level = 0;
	max_historical_logs = 0;
	historical_sequence_number = 0;
}

template <typename K, typename AD>
ClassAdLog<K,AD>::~ClassAdLog()
{
	if (active_transaction) delete active_transaction;

	// cache the effective table entry maker for use in the loop.
	const ConstructLogEntry & dtor = this->GetTableEntryMaker();

	// HashTable class will not delete the ClassAd pointers we have
	// inserted, so we delete them here...
	table.startIterations();
	AD ad;
	K key;
	while (table.iterate(key, ad) == 1) {
		ClassAd * cad = ad;
		dtor.Delete(cad);
	}
	// if we have an allocated table entry maker, delete it now.
	if (make_table_entry && make_table_entry != &DefaultMakeClassAdLogTableEntry) {
		delete make_table_entry;
		make_table_entry = NULL;
	}
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::AppendLog(LogRecord *log)
{
	if (active_transaction) {
		if (active_transaction->EmptyTransaction()) {
			LogBeginTransaction *l = new LogBeginTransaction;
			active_transaction->AppendLog(l);
		}
		active_transaction->AppendLog(log);
	} else {
			//MD: using file pointer
		if (log_fp!=NULL) {
			if (log->Write(log_fp) < 0) {
				EXCEPT("write to %s failed, errno = %d", logFilename(), errno);
			}
			if( m_nondurable_level == 0 ) {
				ForceLog();  // flush and fsync
			}
		}
		ClassAdLogTable<K,AD> la(table);
		log->Play((void *)&la);
		delete log;
	}
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::FlushLog()
{
	int err = FlushClassAdLog(log_fp, false);
	if (err) {
		EXCEPT("flush to %s failed, errno = %d", logFilename(), err);
	}
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::ForceLog()
{
	// Force log changes to disk.  This involves first flushing
	// the log from memory buffers, then fsyncing to disk.
	int err = FlushClassAdLog(log_fp, true);
	if (err) {
		EXCEPT("fsync of %s failed, errno = %d", logFilename(), err);
	}
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::SaveHistoricalLogs()
{
	return SaveHistoricalClassAdLogs(logFilename(), max_historical_logs, historical_sequence_number);
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::TruncLog()
{
	dprintf(D_ALWAYS,"About to rotate ClassAd log %s\n",logFilename());

	if(!SaveHistoricalLogs()) {
		dprintf(D_ALWAYS,"Skipping log rotation, because saving of historical log failed for %s.\n",logFilename());
		return false;
	}

	MyString errmsg;
	ClassAdLogTable<K,AD> la(table); // this gives the ability to add & remove table items.
	bool rotated = TruncateClassAdLog(logFilename(),
		la, this->GetTableEntryMaker(),
		log_fp, historical_sequence_number, m_original_log_birthdate,
		errmsg);
	if ( ! log_fp) {
		// if after rotation, the log is no longer open, the the failure is fatal, and we must except
		EXCEPT("%s", errmsg.c_str());
	}
	if ( ! errmsg.empty()) {
		dprintf(D_ALWAYS, "%s", errmsg.c_str());
	}

	return rotated;
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::LogState(FILE *fp)
{
	MyString errmsg;
	ClassAdLogTable<K,AD> la(table); // this gives the ability to add & remove table items.
	bool success = WriteClassAdLogState(fp, logFilename(),
		historical_sequence_number, m_original_log_birthdate,
		la, this->GetTableEntryMaker(),
		errmsg);
	if (! success) {
		EXCEPT("%s", errmsg.c_str());
	}
}

// Transaction methods
template <typename K, typename AD>
int
ClassAdLog<K,AD>::IncNondurableCommitLevel()
{
	return m_nondurable_level++;
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::DecNondurableCommitLevel(int old_level)
{
	if( --m_nondurable_level != old_level ) {
		EXCEPT("ClassAdLog::DecNondurableCommitLevel(%d) with existing level %d",
			   old_level, m_nondurable_level+1);
	}
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::BeginTransaction()
{
	ASSERT(!active_transaction);
	active_transaction = new Transaction();
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::AbortTransaction()
{
	// Sometimes we do an AbortTransaction() when we don't know if there was
	// an active transaction.  This is allowed.
	if (active_transaction) {
		delete active_transaction;
		active_transaction = NULL;
		return true;
	}
	return false;
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::CommitTransaction(const char * comment /*=NULL*/)
{
	// Sometimes we do a CommitTransaction() when we don't know if there was
	// an active transaction.  This is allowed.
	if (!active_transaction) return;
	if (!active_transaction->EmptyTransaction()) {
		LogEndTransaction *log = new LogEndTransaction;
		log->set_comment(comment);
		active_transaction->AppendLog(log);
		bool nondurable = m_nondurable_level > 0;
		ClassAdLogTable<K,AD> la(table);
		active_transaction->Commit(log_fp, logFilename(), &la, nondurable );
	}
	delete active_transaction;
	active_transaction = NULL;
}

template <typename K, typename AD>
void
ClassAdLog<K,AD>::CommitNondurableTransaction(const char * comment /*=NULL*/)
{
	int old_level = IncNondurableCommitLevel();
	CommitTransaction(comment);
	DecNondurableCommitLevel( old_level );
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::AdExistsInTableOrTransaction(const K& key)
{
	bool adexists = false;

		// first see if it exists in the "commited" hashtable
	AD ad = NULL;
	if ( table.lookup(key, ad) >= 0 && ad ) {
		adexists = true;
	}

		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return adexists;
	}

	std::string keystr = key;

		// see what is going on in any current transaction
	for (LogRecord *log = active_transaction->FirstEntry(keystr.c_str()); log;
		 log = active_transaction->NextEntry()) 
	{
		switch (log->get_op_type()) {
		case CondorLogOp_NewClassAd: {
			adexists = true;
			break;
		}
		case CondorLogOp_DestroyClassAd: {
			adexists = false;
			break;
		}
		default:
			break;
		}
	}

	return adexists;
}


template <typename K, typename AD>
int 
ClassAdLog<K,AD>::LookupInTransaction(const K& key, const char *name, char *&val)
{
	ClassAd *ad = NULL;

	if (!name) return 0;

	// note: this should not return ad != NULL if name is not null.
	return ExamineTransaction(key, name, val, ad);
}


template <typename K, typename AD>
int
ClassAdLog<K,AD>::ExamineTransaction(const K& key, const char *name, char *&val, ClassAd* &ad)
{
	if (!active_transaction) return 0;
	const std::string keystr = key;
	return ExamineLogTransaction(active_transaction, this->GetTableEntryMaker(), keystr.c_str(), name, val, ad);
}

template <typename K, typename AD>
Transaction *
ClassAdLog<K,AD>::getActiveTransaction()
{
	Transaction *ret_value = active_transaction;
	active_transaction = NULL;	// it is IMPORTANT that we reset active_tranasction to NULL here!
	return ret_value;
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::setActiveTransaction(Transaction* & transaction)
{
	if ( active_transaction ) {
		return false;
	}

	active_transaction = transaction;

	transaction = NULL;		// make certain caller doesn't mess w/ the handle 

	return true;
}

template <typename K, typename AD>
int ClassAdLog<K,AD>::SetTransactionTriggers(int mask)
{
	if (!active_transaction) return 0;
	return active_transaction->SetTriggers(mask);
}

template <typename K, typename AD>
int ClassAdLog<K,AD>::GetTransactionTriggers()
{
	if (!active_transaction) return 0;
	return active_transaction->GetTriggers();
}


template <typename K, typename AD>
bool
ClassAdLog<K,AD>::AddAttrsFromTransaction(const K& key, ClassAd &ad)
{
		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return false;
	}
	const std::string keystr = key;
	return AddAttrsFromLogTransaction(active_transaction, this->GetTableEntryMaker(), keystr.c_str(), ad);
}

template <typename K, typename AD>
bool
ClassAdLog<K,AD>::AddAttrNamesFromTransaction(const K &key, classad::References & attrs)
{
		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return false;
	}
	const std::string keystr = key;
	return AddAttrNamesFromLogTransaction(active_transaction, keystr.c_str(), attrs);
}


template <typename K, typename AD>
void ClassAdLog<K,AD>::ListNewAdsInTransaction( std::list<std::string> &new_keys )
{
	if( !active_transaction ) {
		return;
	}

	active_transaction->InTransactionListKeysWithOpType( CondorLogOp_NewClassAd, new_keys );
}

template <typename K, typename AD>
bool ClassAdLog<K,AD>::GetTransactionKeys( std::set<std::string> &keys )
{
	if ( ! active_transaction) { return false; }
	active_transaction->KeysInTransaction( keys );
	return true;
}


#endif
