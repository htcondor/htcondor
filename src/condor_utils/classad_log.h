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
#include "classad_hashtable.h"
#include "log_transaction.h"

extern const char *EMPTY_CLASSAD_TYPE_NAME;

// used to store a ClassAd and some other stuff in a condor hashtable.
#define USE_DERIVED_AD_AND_STUFF 1
class ClassAdAndStuff : public ClassAd {
public:
	int id;

	ClassAdAndStuff(int _id=0) : id(_id) {}
	//ClassAdAndStuff( const ClassAd &ad );
	//ClassAdAndStuff( const classad::ClassAd &ad );
	virtual ~ClassAdAndStuff() {};
};
typedef ClassAdAndStuff* AD_AND_STUFF;


template <typename AD>
class ConstructClassAdLogTableEntry : public ConstructLogEntry
{
public:
	virtual ClassAd* New() const { return new AD(); }
	virtual void Delete(ClassAd*& val) const { delete val; }
};

template <>
class ConstructClassAdLogTableEntry<ClassAd*> : public ConstructLogEntry
{
public:
	virtual ClassAd* New() const { return new ClassAd(); }
	virtual void Delete(ClassAd*& val) const { delete val; }
};

#ifdef USE_DERIVED_AD_AND_STUFF
template <>
class ConstructClassAdLogTableEntry<ClassAdAndStuff*> : public ConstructLogEntry
{
public:
	virtual ClassAd* New() const { return new ClassAdAndStuff(); }
	virtual void Delete(ClassAd* &val) const { delete val; }
};
#endif

const ConstructClassAdLogTableEntry<ClassAd*> DefaultMakeClassAdLogTableEntry;

template <typename K, typename AltK, typename AD>
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

		public:
			filter_iterator(ClassAdLog<K,AltK,AD> &log, const classad::ExprTree *requirements, int timeslice_ms, bool invalid=false)
				: m_table(&log.table)
				, m_cur(log.table.begin())
				, m_found_ad(false)
				, m_requirements(requirements)
				, m_timeslice_ms(timeslice_ms)
				, m_done(invalid) {}
			filter_iterator(const filter_iterator &other)
				: m_table(other.m_table)
				, m_cur(other.m_cur)
				, m_found_ad(other.m_found_ad)
				, m_requirements(other.m_requirements)
				, m_timeslice_ms(other.m_timeslice_ms)
				, m_done(other.m_done) {}

			~filter_iterator() {}
			AD operator *() const {
				if (m_done || (m_cur == m_table->end()) || !m_found_ad)
					return AD(NULL);
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
	};


	void AppendLog(LogRecord *log);	// perform a log operation
	bool TruncLog();				// clean log file on disk

	void BeginTransaction();
	bool AbortTransaction();
	void CommitTransaction();
	void CommitNondurableTransaction();
	bool InTransaction() { return active_transaction != NULL; }

	/** Get a list of all new keys created in this transaction
		@param new_keys List object to populate
	*/
	void ListNewAdsInTransaction( std::list<std::string> &new_keys );

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

	bool AdExistsInTableOrTransaction(AltK key);

	// returns 1 and sets val if corresponding SetAttribute found
	// returns 0 if no SetAttribute found
	// return -1 if DeleteAttribute or DestroyClassAd found
	int LookupInTransaction(AltK key, const char *name, char *&val);

	// insert into the given ad any attributes found in the uncommitted transaction
	// cache that match the key.  return true if any attributes were
	// added into the ad, false if not.
	bool AddAttrsFromTransaction(AltK key, ClassAd &ad);

	HashTable<K,AD> table;

	// user-replacable helper class for creating and destroying values for the hashtable
	// this allows a user of the classad log to put a class derived from ClassAd in the hashtable
	// without the ClassAdLog code needing to know all about it.
	const ConstructLogEntry * make_table_entry;
	const ConstructLogEntry& GetTableEntryMaker() { if (make_table_entry) return *make_table_entry; return DefaultMakeClassAdLogTableEntry; }

	// When the log is truncated (i.e. compacted), old logs
	// may be saved, up to some limit.  The default is not
	// to save any history (max = 0).
	void SetMaxHistoricalLogs(int max);
	int GetMaxHistoricalLogs();

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

	int ExamineTransaction(AltK key, const char *name, char *&val, ClassAd* &ad);


private:
	void LogState(FILE* fp);
	FILE* log_fp;

	char const *logFilename() { return log_filename_buf.Value(); }
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

	unsigned long get_historical_sequence_number() {return historical_sequence_number;}
	time_t get_timestamp() {return timestamp;}

private:
	virtual int WriteBody(FILE *fp);
	virtual int ReadBody(FILE *fp);

	virtual char const *get_key() {return NULL;}

	unsigned long historical_sequence_number;
	time_t timestamp; //when was the the first record originally written,
					  // regardless of how many times the log has rotated
};

// this class is the interface that is consumed by classes in this file derived LogRecord
class LoggableClassAdTable {
public:
	virtual ~LoggableClassAdTable() {};
	virtual bool lookup(const char * key, ClassAd*& ad) = 0;
	virtual bool remove(const char * key) = 0;
	virtual bool insert(const char * key, ClassAd* ad) = 0;
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
	virtual bool lookup(const char * key, ClassAd*& ad) { AD Ad; int iret = table.lookup(K(key), Ad); ad=Ad; return iret >= 0; }
	virtual bool remove(const char * key) { return table.remove(K(key)) >= 0; }
	virtual bool insert(const char * key, ClassAd * ad) { int iret = table.insert(K(key), AD(ad)); return iret >= 0; }
protected:
	HashTable<K,AD> & table;
};

#ifdef USE_DERIVED_AD_AND_STUFF
template <>
class ClassAdLogTable<JOB_ID_KEY,AD_AND_STUFF> : public LoggableClassAdTable {
public:
	ClassAdLogTable(HashTable<JOB_ID_KEY,AD_AND_STUFF> & _table) : table(_table) {}
	virtual ~ClassAdLogTable() {};
	virtual bool lookup(const char * key, ClassAd*& ad) { AD_AND_STUFF Ad; int iret = table.lookup(JOB_ID_KEY(key), Ad); ad=Ad; return iret >= 0; }
	virtual bool remove(const char * key) { return table.remove(JOB_ID_KEY(key)) >= 0; }
	virtual bool insert(const char * key, ClassAd * ad) {
		AD_AND_STUFF Ad = new ClassAdAndStuff();
		Ad->Update(*ad); delete ad; // TODO: transfer ownership of attributes from ad to Ad
		int iret = table.insert(JOB_ID_KEY(key), Ad);
		return iret >= 0;
	}
protected:
	HashTable<JOB_ID_KEY,AD_AND_STUFF> & table;
};
#endif

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
	virtual int WriteBody(FILE* fp) { int r=fwrite(key, sizeof(char), strlen(key), fp); return r < ((int)strlen(key)) ? -1 : r;}
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
	LogEndTransaction() { op_type = CondorLogOp_EndTransaction; }
	virtual ~LogEndTransaction(){};

	int Play(void *data_structure); // data_structure should be of type LoggableClassAdTable *
private:
	virtual int WriteBody(FILE* /*fp*/) {return 0;}
	virtual int ReadBody(FILE* fp);

	virtual char const *get_key() {return NULL;}
};

LogRecord* InstantiateLogEntry(FILE* fp, unsigned long recnum, int type, const ConstructLogEntry & ctor);

#endif
