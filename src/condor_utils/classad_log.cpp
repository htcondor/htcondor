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
#include "classad_log.h"
#include "condor_debug.h"
#include "util_lib_proto.h"
#include "classad_merge.h"
#include "condor_fsync.h"

#if defined(HAVE_DLOPEN)
#include "ClassAdLogPlugin.h"
#endif

// explicitly instantiate the HashTable template

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
/************************************************************/

#define CLASSAD_LOG_HASHTABLE_SIZE 20000

const char *EMPTY_CLASSAD_TYPE_NAME = "(empty)";

ClassAdLog::ClassAdLog() : table(CLASSAD_LOG_HASHTABLE_SIZE, hashFunction)
{
	active_transaction = NULL;
	log_fp = NULL;
	m_nondurable_level = 0;
	max_historical_logs = 0;
	historical_sequence_number = 0;
}

ClassAdLog::ClassAdLog(const char *filename,int max_historical_logs_arg) : table(CLASSAD_LOG_HASHTABLE_SIZE, hashFunction)
{
	log_filename_buf = filename;
	active_transaction = NULL;
	m_nondurable_level = 0;

	this->max_historical_logs = max_historical_logs_arg;
	historical_sequence_number = 1;
	m_original_log_birthdate = time(NULL);

	int log_fd = safe_open_wrapper_follow(logFilename(), O_RDWR | O_CREAT | O_LARGEFILE, 0600);
	if (log_fd < 0) {
		EXCEPT("failed to open log %s, errno = %d", logFilename(), errno);
	}

	log_fp = fdopen(log_fd, "r+");
	if (log_fp == NULL) {
		EXCEPT("failed to fdopen log %s, errno = %d", logFilename(), errno);
	}


	// Read all of the log records
	LogRecord		*log_rec;
	unsigned long count = 0;
	bool is_clean = true; // was cleanly closed (until we find out otherwise)
	bool requires_successful_cleaning = false;
	long long next_log_entry_pos = 0;
    long long curr_log_entry_pos = 0;
	while ((log_rec = ReadLogEntry(log_fp, 1+count, InstantiateLogEntry)) != 0) {
        curr_log_entry_pos = next_log_entry_pos;
		next_log_entry_pos = ftell(log_fp);
		count++;
		switch (log_rec->get_op_type()) {
        case CondorLogOp_Error:
            // this is defensive, ought to be caught in InstantiateLogEntry()
            EXCEPT("ERROR: transaction record %lu was bad (byte offset %lld)\n", count, curr_log_entry_pos);
            break;
		case CondorLogOp_BeginTransaction:
			// this file contains transactions, so it must not
			// have been cleanly shut down
			is_clean = false;
			if (active_transaction) {
				dprintf(D_ALWAYS, "Warning: Encountered nested transactions in %s, "
						"log may be bogus...", filename);
			} else {
				active_transaction = new Transaction();
			}
			delete log_rec;
			break;
		case CondorLogOp_EndTransaction:
			if (!active_transaction) {
				dprintf(D_ALWAYS, "Warning: Encountered unmatched end transaction in %s, "
						"log may be bogus...", filename);
			} else {
				active_transaction->Commit(NULL, (void *)&table); // commit in memory only
				delete active_transaction;
				active_transaction = NULL;
			}
			delete log_rec;
			break;
		case CondorLogOp_LogHistoricalSequenceNumber:
			if(count != 1) {
				dprintf(D_ALWAYS, "Warning: Encountered historical sequence number after first log entry (entry number = %ld)\n",count);
			}
			historical_sequence_number = ((LogHistoricalSequenceNumber *)log_rec)->get_historical_sequence_number();
			m_original_log_birthdate = ((LogHistoricalSequenceNumber *)log_rec)->get_timestamp();
			delete log_rec;
			break;
		default:
			if (active_transaction) {
				active_transaction->AppendLog(log_rec);
			} else {
				log_rec->Play((void *)&table);
				delete log_rec;
			}
		}
	}
	long long final_log_entry_pos = ftell(log_fp);
	if( next_log_entry_pos != final_log_entry_pos ) {
		// The log file has a broken line at the end so we _must_
		// _not_ write anything more into this log.
		// (Alternately, we could try to clear out the broken entry
		// and continue writing into this file, but since we are about to
		// rotate the log anyway, we may as well just require the rotation
		// to be successful.  In the case where rotation fails, we will
		// probably soon fail to write to the log file anyway somewhere else.)
		dprintf(D_ALWAYS,"Detected unterminated log entry in ClassAd Log %s."
				" Forcing rotation.\n", logFilename());
		requires_successful_cleaning = true;
	}
	if (active_transaction) {	// abort incomplete transaction
		delete active_transaction;
		active_transaction = NULL;

		if( !requires_successful_cleaning ) {
			// For similar reasons as with broken log entries above,
			// we need to force rotation.
			dprintf(D_ALWAYS,"Detected unterminated transaction in ClassAd Log"
					"%s. Forcing rotation.\n", logFilename());
			requires_successful_cleaning = true;
		}
	}
	if(!count) {
		log_rec = new LogHistoricalSequenceNumber( historical_sequence_number, m_original_log_birthdate );
		if (log_rec->Write(log_fp) < 0) {
			EXCEPT("write to %s failed, errno = %d", logFilename(), errno);
		}
	}
	if( !is_clean || requires_successful_cleaning ) {
		if( !TruncLog() && requires_successful_cleaning ) {
			EXCEPT("Failed to rotate ClassAd log %s.\n", logFilename());
		}
	}
}

ClassAdLog::~ClassAdLog()
{
	if (active_transaction) delete active_transaction;

	// HashTable class will not delete the ClassAd pointers we have
	// inserted, so we delete them here...
	table.startIterations();
	ClassAd *ad;
	HashKey key;
	while (table.iterate(key, ad) == 1) {
		delete ad;
	}
}
void
ClassAdLog::AppendLog(LogRecord *log)
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
					//MD: flushing data -- using a file pointer
				if (fflush(log_fp) !=0){
					EXCEPT("flush to %s failed, errno = %d", logFilename(), errno);
				}
					//MD: syncing the data as done before
				if (condor_fsync(fileno(log_fp)) < 0) {
					EXCEPT("fsync of %s failed, errno = %d", logFilename(), errno);
				}
			}
		}
		log->Play((void *)&table);
		delete log;
	}
}

void
ClassAdLog::FlushLog()
{
	if (log_fp!=NULL) {
		if (fflush(log_fp) !=0){
			EXCEPT("flush to %s failed, errno = %d", logFilename(), errno);
		}
	}
}

bool
ClassAdLog::SaveHistoricalLogs()
{
	if(!max_historical_logs) return true;

	MyString new_histfile;
	if(!new_histfile.formatstr("%s.%lu",logFilename(),historical_sequence_number))
	{
		dprintf(D_ALWAYS,"Aborting save of historical log: out of memory.\n");
		return false;
	}

	dprintf(D_FULLDEBUG,"About to save historical log %s\n",new_histfile.Value());

	if( hardlink_or_copy_file(logFilename(), new_histfile.Value()) < 0) {
		dprintf(D_ALWAYS,"Failed to copy %s to %s.\n",logFilename(),new_histfile.Value());
		return false;
	}

	MyString old_histfile;
	if(!old_histfile.formatstr("%s.%lu",logFilename(),historical_sequence_number - max_historical_logs))
	{
		dprintf(D_ALWAYS,"Aborting cleanup of historical logs: out of memory.\n");
		return true; // this is not a fatal error
	}

	if( unlink(old_histfile.Value()) == 0 ) {
		dprintf(D_FULLDEBUG,"Removed historical log %s.\n",old_histfile.Value());
	}
	else {
		// It's ok if the old file simply doesn't exist.
		if( errno != ENOENT ) {
			// Otherwise, it's not a fatal error, but definitely odd that
			// we failed to remove it.
			dprintf(D_ALWAYS,"WARNING: failed to remove '%s': %s\n",old_histfile.Value(),strerror(errno));
		}
		return true; // this is not a fatal error
	}
	return true;
}

void
ClassAdLog::SetMaxHistoricalLogs(int max) {
	this->max_historical_logs = max;
}

int
ClassAdLog::GetMaxHistoricalLogs() {
	return max_historical_logs;
}

bool
ClassAdLog::TruncLog()
{
	MyString	tmp_log_filename;
	int new_log_fd;
	FILE *new_log_fp;

	dprintf(D_ALWAYS,"About to rotate ClassAd log %s\n",logFilename());

	if(!SaveHistoricalLogs()) {
		dprintf(D_ALWAYS,"Skipping log rotation, because saving of historical log failed for %s.\n",logFilename());
		return false;
	}

	tmp_log_filename.formatstr( "%s.tmp", logFilename());
	new_log_fd = safe_open_wrapper_follow(tmp_log_filename.Value(), O_RDWR | O_CREAT | O_LARGEFILE, 0600);
	if (new_log_fd < 0) {
		dprintf(D_ALWAYS, "failed to rotate log: safe_open_wrapper(%s) returns %d\n",
				tmp_log_filename.Value(), new_log_fd);
		return false;
	}

	new_log_fp = fdopen(new_log_fd, "r+");
	if (new_log_fp == NULL) {
		dprintf(D_ALWAYS, "failed to rotate log: fdopen(%s) returns NULL\n",
				tmp_log_filename.Value());
		return false;
	}


	// Now it is time to move courageously into the future.
	historical_sequence_number++;

	LogState(new_log_fp);
	fclose(log_fp);
	log_fp = NULL;
	fclose(new_log_fp);	// avoid sharing violation on move
	if (rotate_file(tmp_log_filename.Value(), logFilename()) < 0) {
		dprintf(D_ALWAYS, "failed to rotate job queue log!\n");

		// Beat a hasty retreat into the past.
		historical_sequence_number--;

		int log_fd = safe_open_wrapper_follow(logFilename(), O_RDWR | O_APPEND | O_LARGEFILE, 0600);
		if (log_fd < 0) {
			EXCEPT("failed to reopen log %s, errno = %d after failing to rotate log.",logFilename(),errno);
		}

		log_fp = fdopen(log_fd, "a+");
		if (log_fp == NULL) {
			EXCEPT("failed to refdopen log %s, errno = %d after failing to rotate log.",logFilename(),errno);
		}

		return false;
	}
	int log_fd = safe_open_wrapper_follow(logFilename(), O_RDWR | O_APPEND | O_LARGEFILE, 0600);
	if (log_fd < 0) {
		EXCEPT( "failed to open log in append mode: "
			"safe_open_wrapper(%s) returns %d\n", logFilename(), log_fd);
	}
	log_fp = fdopen(log_fd, "a+");
	if (log_fp == NULL) {
		close(log_fd);
		EXCEPT("failed to fdopen log in append mode: "
			"fdopen(%s) returns %d\n", logFilename(), log_fd);
	}

	return true;
}

int
ClassAdLog::IncNondurableCommitLevel()
{
	return m_nondurable_level++;
}

void
ClassAdLog::DecNondurableCommitLevel(int old_level)
{
	if( --m_nondurable_level != old_level ) {
		EXCEPT("ClassAdLog::DecNondurableCommitLevel(%d) with existing level %d\n",
			   old_level, m_nondurable_level+1);
	}
}

void
ClassAdLog::BeginTransaction()
{
	ASSERT(!active_transaction);
	active_transaction = new Transaction();
}

bool
ClassAdLog::AbortTransaction()
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

void
ClassAdLog::CommitTransaction()
{
	// Sometimes we do a CommitTransaction() when we don't know if there was
	// an active transaction.  This is allowed.
	if (!active_transaction) return;
	if (!active_transaction->EmptyTransaction()) {
		LogEndTransaction *log = new LogEndTransaction;
		active_transaction->AppendLog(log);
		bool nondurable = m_nondurable_level > 0;
		active_transaction->Commit(log_fp, (void *)&table, nondurable );
	}
	delete active_transaction;
	active_transaction = NULL;
}

void
ClassAdLog::CommitNondurableTransaction()
{
	int old_level = IncNondurableCommitLevel();
	CommitTransaction();
	DecNondurableCommitLevel( old_level );
}

bool
ClassAdLog::AdExistsInTableOrTransaction(const char *key)
{
	bool adexists = false;

		// first see if it exists in the "commited" hashtable
	HashKey hkey(key);
	ClassAd *ad = NULL;
	table.lookup(hkey, ad);
	if ( ad ) {
		adexists = true;
	}

		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return adexists;
	}

		// see what is going on in any current transaction
	for (LogRecord *log = active_transaction->FirstEntry(key); log; 
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


int 
ClassAdLog::LookupInTransaction(const char *key, const char *name, char *&val)
{
	ClassAd *ad = NULL;

	if (!name) return 0;

	return ExamineTransaction(key, name, val, ad);
}

bool
ClassAdLog::AddAttrsFromTransaction(const char *key, ClassAd &ad)
{
	char *val = NULL;
	ClassAd *attrsFromTransaction;

	if ( !key ) {
		return false;
	}

		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return false;
	}

		// see what is going on in any current transaction
	attrsFromTransaction = NULL;
	ExamineTransaction(key,NULL,val,attrsFromTransaction);
	if ( attrsFromTransaction ) {
		MergeClassAds(&ad,attrsFromTransaction,true);
		delete attrsFromTransaction;
		return true;
	}
	return false;
}

int
ClassAdLog::ExamineTransaction(const char *key, const char *name, char *&val, ClassAd* &ad)
{
	bool AdDeleted=false, ValDeleted=false, ValFound=false;
	int attrsAdded = 0;

	if (!active_transaction) return 0;

	for (LogRecord *log = active_transaction->FirstEntry(key); log; 
		 log = active_transaction->NextEntry()) {

		switch (log->get_op_type()) {
		case CondorLogOp_NewClassAd: {
			if (AdDeleted) {	// check to see if ad is created after a delete
				AdDeleted = false;
			}
			break;
		}
		case CondorLogOp_DestroyClassAd: {
			AdDeleted = true;
			if ( ad ) {
				delete ad;
				ad = NULL;
				attrsAdded = 0;
			}
			break;
		}
		case CondorLogOp_SetAttribute: {
			char const *lname = ((LogSetAttribute *)log)->get_name();
			if (name && strcasecmp(lname, name) == 0) {
				if (ValFound) {
					free(val);
				}
				val = strdup(((LogSetAttribute *)log)->get_value());
				ValFound = true;
				ValDeleted = false;
			}
			if (!name) {
				if ( !ad ) {
					ad = new ClassAd;
					ASSERT(ad);
				}
				if (val) {
					free(val);
					val = NULL;
				}
                ExprTree* expr = ((LogSetAttribute *)log)->get_expr();
                if (expr) {
		    expr = expr->Copy();
                    ad->Insert(lname, expr, false);
                } else {
                    val = strdup(((LogSetAttribute *)log)->get_value());
                    ad->AssignExpr(lname, val);
                }
				attrsAdded++;
			}
			break;
		}
		case CondorLogOp_DeleteAttribute: {
			char const *lname = ((LogDeleteAttribute *)log)->get_name();
			if (name && strcasecmp(lname, name) == 0) {
				if (ValFound) {
					free(val);
				}
				ValFound = false;
				ValDeleted = true;
			}
			if (!name) {
				if (ad) {
					ad->Delete(lname);
					attrsAdded--;
				}
			}
			break;
		}
		default:
			break;
		}
	}

	if ( name ) {
		if (AdDeleted || ValDeleted) return -1;
		if (ValFound) return 1;
		return 0;
	} else {
		if (attrsAdded < 0 ) {
			return 0;
		}
		return attrsAdded;
	}
}

Transaction *
ClassAdLog::getActiveTransaction()
{
	Transaction *ret_value = active_transaction;
	active_transaction = NULL;	// it is IMPORTANT that we reset active_tranasction to NULL here!
	return ret_value;
}

bool
ClassAdLog::setActiveTransaction(Transaction* & transaction)
{
	if ( active_transaction ) {
		return false;
	}

	active_transaction = transaction;

	transaction = NULL;		// make certain caller doesn't mess w/ the handle 

	return true;
}


void
ClassAdLog::LogState(FILE *fp)
{
	LogRecord	*log=NULL;
	ClassAd		*ad=NULL;
	ExprTree	*expr=NULL;
	HashKey		hashval;
	MyString	key;
	const char	*attr_name = NULL;

	// This must always be the first entry in the log.
	log = new LogHistoricalSequenceNumber( historical_sequence_number, m_original_log_birthdate );
	if (log->Write(fp) < 0) {
		EXCEPT("write to %s failed, errno = %d", logFilename(), errno);
	}
	delete log;

	table.startIterations();
	while(table.iterate(ad) == 1) {
		table.getCurrentKey(hashval);
		hashval.sprint(key);
		log = new LogNewClassAd(key.Value(), ad->GetMyTypeName(), ad->GetTargetTypeName());
		if (log->Write(fp) < 0) {
			EXCEPT("write to %s failed, errno = %d", logFilename(), errno);
		}
		delete log;
			// Unchain the ad -- we just want to write out this ads exprs,
			// not all the exprs in the chained ad as well.
		AttrList *chain = dynamic_cast<AttrList*>(ad->GetChainedParentAd());
		ad->Unchain();
		ad->ResetName();
		attr_name = ad->NextNameOriginal();
		while (attr_name) {
			expr = ad->LookupExpr(attr_name);
				// This conditional used to check whether the ExprTree is
				// invisible, but no codepath sets any attributes
				// invisible for this call.
			if (expr) {
				log = new LogSetAttribute(key.Value(), attr_name,
										  ExprTreeToString(expr));
				if (log->Write(fp) < 0) {
					EXCEPT("write to %s failed, errno = %d", logFilename(),
						   errno);
				}
				delete log;
			}
			attr_name = ad->NextNameOriginal();
		}
			// ok, now that we're done writing out this ad, restore the chain
		ad->ChainToAd(chain);
	}
	if (fflush(fp) !=0){
	  EXCEPT("fflush of %s failed, errno = %d", logFilename(), errno);
	}
	if (condor_fsync(fileno(fp)) < 0) {
		EXCEPT("fsync of %s failed, errno = %d", logFilename(), errno);
	} 
}

LogHistoricalSequenceNumber::LogHistoricalSequenceNumber(unsigned long historical_sequence_number_arg,time_t timestamp_arg)
{
	op_type = CondorLogOp_LogHistoricalSequenceNumber;
	this->historical_sequence_number = historical_sequence_number_arg;
	this->timestamp = timestamp_arg;
}
int
LogHistoricalSequenceNumber::Play(void *  /*data_structure*/)
{
	// Would like to update ClassAdLog here, but we only have
	// a pointer to the classad hash table, so ClassAdLog must
	// update its own sequence number when it reads this event.
	return 1;
}


int
LogHistoricalSequenceNumber::ReadBody(FILE *fp)
{
	int rval,rval1;
	char *buf = NULL;
	rval = readword(fp, buf);
	if (rval < 0) return rval;
	MSC_SUPPRESS_WARNING_FIXME(6031)// return value of scanf ignored. int64 does not match %lu
	sscanf(buf,"%lu",&historical_sequence_number);
	free(buf);

	rval1 = readword(fp, buf); //the label of the attribute 
				//we ignore it
	if (rval1 < 0) return rval1; 
	free(buf);

	rval1 = readword(fp, buf);
	if (rval1 < 0) return rval1;
	MSC_SUPPRESS_WARNING_FIXME(6031 6328)// return value of scanf ignored. int64 does not match %lu
	sscanf(buf,"%lu",&timestamp);
	free(buf);
	return rval + rval1;
}

int
LogHistoricalSequenceNumber::WriteBody(FILE *fp)
{
	char buf[100];
	snprintf(buf,COUNTOF(buf),"%lu CreationTimestamp %lu",
		historical_sequence_number, (unsigned long)timestamp);
	buf[COUNTOF(buf)-1] = 0; // snprintf not guranteed to null terminate.
	int len = strlen(buf);
	return (fwrite(buf, 1, len, fp) < (unsigned)len) ? -1: len;
}

LogNewClassAd::LogNewClassAd(const char *k, const char *m, const char *t)
{
	op_type = CondorLogOp_NewClassAd;
	key = strdup(k);
	mytype = strdup(m);
	targettype = strdup(t);
}

LogNewClassAd::~LogNewClassAd()
{
	free(key);
	free(mytype);
	free(targettype);
}

int
LogNewClassAd::Play(void *data_structure)
{
	int result;
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	ClassAd	*ad = new ClassAd();
	ad->SetMyTypeName(mytype);
	ad->SetTargetTypeName(targettype);
	result = table->insert(HashKey(key), ad);

#if defined(HAVE_DLOPEN)
	ClassAdLogPluginManager::NewClassAd(key);
#endif

	return result;
}

int
LogNewClassAd::ReadBody(FILE* fp)
{
	int rval, rval1;
	free(key);
	rval = readword(fp, key);
	if (rval < 0) return rval;
	free(mytype);
	rval1 = readword(fp, mytype);
	if( mytype && strcmp(mytype,EMPTY_CLASSAD_TYPE_NAME)==0 ) {
		free(mytype);
		mytype = strdup("");
		ASSERT( mytype );
	}
	if (rval1 < 0) return rval1;
	rval += rval1;
	free(targettype);
	rval1 = readword(fp, targettype);
	if( targettype && strcmp(targettype,EMPTY_CLASSAD_TYPE_NAME)==0 ) {
		free(targettype);
		targettype = strdup("");
		ASSERT( targettype );
	}
	if (rval1 < 0) return rval1;
	return rval + rval1;
}

int
LogNewClassAd::WriteBody(FILE* fp)
{
	int rval, rval1;
	rval = fwrite(key, sizeof(char), strlen(key), fp);
	if (rval < (int)strlen(key)) return -1;
	rval1 = fwrite( " ", sizeof(char), 1, fp);
	if (rval1 < 1) return -1;
	rval += rval1;
	char const *s = mytype;
	if( !s || !s[0] ) {
			// Because writing an empty string would result
			// in a log entry with the wrong number of fields,
			// we write a placeholder.
		s = EMPTY_CLASSAD_TYPE_NAME;
	}
	rval1 = fwrite(s, sizeof(char), strlen(s), fp);
	if (rval1 < (int)strlen(s)) return -1;
	rval += rval1;
	rval1 = fwrite(" ", sizeof(char), 1, fp);
	if (rval1 < 1) return -1;
	rval += rval1;
	s = targettype;
	if( !s || !s[0] ) {
			// Because writing an empty string would result
			// in a log entry with the wrong number of fields,
			// we write a placeholder.
		s = EMPTY_CLASSAD_TYPE_NAME;
	}
	rval1 = fwrite(s, sizeof(char), strlen(s),fp);
	if (rval1 < (int)strlen(s)) return -1;
	return rval + rval1;
}

LogDestroyClassAd::LogDestroyClassAd(const char *k)
{
	op_type = CondorLogOp_DestroyClassAd;
	key = strdup(k);
}


LogDestroyClassAd::~LogDestroyClassAd()
{
	free(key);
}


int
LogDestroyClassAd::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	HashKey hkey(key);
	ClassAd *ad;

	if (table->lookup(hkey, ad) < 0) {
		return -1;
	}

#if defined(HAVE_DLOPEN)
	ClassAdLogPluginManager::DestroyClassAd(key);
#endif

	delete ad;
	return table->remove(hkey);
}

int
LogDestroyClassAd::ReadBody(FILE* fp)
{
	free(key);
	return readword(fp, key);
}

LogSetAttribute::LogSetAttribute(const char *k, const char *n, const char *val, bool dirty)
{
	op_type = CondorLogOp_SetAttribute;
	key = strdup(k);
	name = strdup(n);
	if (val && strlen(val)) {
		value = strdup(val);
	} else {
		value = strdup("UNDEFINED");
	}
	is_dirty = dirty;
    value_expr = NULL;
}


LogSetAttribute::~LogSetAttribute()
{
	free(key);
	free(name);
	free(value);
    if (value_expr != NULL) delete value_expr;
}


int
LogSetAttribute::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	int rval;
	ClassAd *ad = 0;
	if (table->lookup(HashKey(key), ad) < 0)
		return -1;
    if (value_expr) {
        ExprTree * pTree = value_expr->Copy();
        rval = ad->Insert(name, pTree, false);
    } else {
        rval = ad->AssignExpr(name, value);
    }
	ad->SetDirtyFlag(name, is_dirty);

#if defined(HAVE_DLOPEN)
	ClassAdLogPluginManager::SetAttribute(key, name, value);
#endif

	return rval;
}

int
LogSetAttribute::WriteBody(FILE* fp)
{
	int		rval, rval1, len;

	// Ensure no newlines sneak through (as they're a record seperator)
	if( strchr(key, '\n') || strchr(name, '\n') || strchr(value, '\n') ) {
		dprintf(D_ALWAYS, "Refusing attempt to add '%s' = '%s' to record '%s' as it contains a newline, which is not allowed.\n", name, value, key);
		return -1;
	}

	len = strlen(key);
	rval = fwrite(key, sizeof(char), len, fp);
	if (rval < len) {
		return -1;
	}
	rval1 = fwrite(" ", sizeof(char), 1,fp);
	if (rval1 < 1) {
		return -1;
	}
	rval1 += rval;
	len = strlen(name);
	rval = fwrite(name, sizeof(char), len, fp);
	if (rval < len) {
		return -1;
	}
	rval1 += rval;
	rval = fwrite( " ",sizeof(char), 1, fp);
	if (rval < 1) {
		return -1;
	}
	rval1 += rval;
	len = strlen(value);
	rval = fwrite( value, sizeof(char), len,fp);
	if (rval < len) {
		return -1;
	}
	return rval1 + rval;
}

int
LogSetAttribute::ReadBody(FILE* fp)
{
	int rval, rval1;

	free(key);
	rval1 = readword(fp, key);
	if (rval1 < 0) {
		return rval1;
	}

	free(name);
	rval = readword(fp, name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	free(value);
	rval = readline(fp, value);
	if (rval < 0) {
		return rval;
	}

    if (value_expr) delete value_expr;
    value_expr = NULL;
    if (ParseClassAdRvalExpr(value, value_expr)) {
        if (value_expr) delete value_expr;
        value_expr = NULL;
        if (param_boolean("CLASSAD_LOG_STRICT_PARSING", true)) {
            return -1;
        } else {
            dprintf(D_ALWAYS, "WARNING: strict classad parsing failed for expression: \"%s\"\n", value);
        }
    }
	return rval + rval1;
}


LogDeleteAttribute::LogDeleteAttribute(const char *k, const char *n)
{
	op_type = CondorLogOp_DeleteAttribute;
	key = strdup(k);
	name = strdup(n);
}


LogDeleteAttribute::~LogDeleteAttribute()
{
	free(key);
	free(name);
}


int
LogDeleteAttribute::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	ClassAd *ad = 0;
	if (table->lookup(HashKey(key), ad) < 0)
		return -1;

#if defined(HAVE_DLOPEN)
	ClassAdLogPluginManager::DeleteAttribute(key, name);
#endif

	return ad->Delete(name);
}

int
LogDeleteAttribute::WriteBody(FILE* fp)
{
	int		rval, rval1, len;

	len = strlen(key);
	rval = fwrite(key, sizeof(char), len, fp);
	if (rval < len) {
		return -1;
	}
	rval1 = fwrite(" ", sizeof(char), 1, fp);
	if (rval1 < 1) {
		return -1;
	}
	rval1 += rval;
	len = strlen(name);
	rval = fwrite(name, sizeof(char), len, fp);
	if (rval < len) {
		return -1;
	}
	return rval1 + rval;
}

int 
LogBeginTransaction::ReadBody(FILE* fp)
{
	char 	ch;
	int		rval = fread( &ch, sizeof(char), 1, fp);
	if( rval < 1 || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}

int 
LogEndTransaction::ReadBody( FILE* fp )
{
	char 	ch;
	int		rval = fread( &ch, sizeof(char), 1, fp );
	if( rval < 1 || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}

int
LogDeleteAttribute::ReadBody(FILE* fp)
{
	int rval, rval1;

	free(key);
	rval1 = readword(fp, key);
	if (rval1 < 0) {
		return rval1;
	}

	free(name);
	rval = readword(fp, name);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
}

LogRecord	*
InstantiateLogEntry(FILE *fp, unsigned long recnum, int type)
{
	LogRecord	*log_rec;

	switch(type) {
        case CondorLogOp_Error:
            log_rec = new LogRecordError();
            break;
	    case CondorLogOp_NewClassAd:
		    log_rec = new LogNewClassAd("", "", "");
			break;
	    case CondorLogOp_DestroyClassAd:
		    log_rec = new LogDestroyClassAd("");
			break;
	    case CondorLogOp_SetAttribute:
		    log_rec = new LogSetAttribute("", "", "");
			break;
	    case CondorLogOp_DeleteAttribute:
		    log_rec = new LogDeleteAttribute("", "");
			break;
		case CondorLogOp_BeginTransaction:
			log_rec = new LogBeginTransaction();
			break;
		case CondorLogOp_EndTransaction:
			log_rec = new LogEndTransaction();
			break;
		case CondorLogOp_LogHistoricalSequenceNumber:
			log_rec = new LogHistoricalSequenceNumber(0,0);
			break;
	    default:
		    return NULL;
			break;
	}

	long long pos = ftell(fp);

	// Check if we got a bogus record indicating a bad log file.  There are two basic
    // failure modes.  The first mode is some kind of parse failure that occurs at the
    // end of the log, not followed by an end-of-transaction operation.  In this case, the
    // incomplete transaction at the end is skipped and ignored with a warning.  The second
    // mode is a failure that occurs inside a complete transaction (one with an end-of-
    // transaction op).  A complete transaction with corruption is unrecoverable, and 
    // causes a fatal exception.
	if (log_rec->ReadBody(fp) < 0  ||  log_rec->get_op_type() == CondorLogOp_Error) {
        dprintf(D_ALWAYS, "WARNING: Encountered corrupt log record %lu (byte offset %lld)\n", recnum, pos);

		char	line[ATTRLIST_MAX_EXPRESSION + 64];
		int		op;

		delete log_rec;
		if( !fp ) {
			EXCEPT("Error: failed fdopen() while recovering from corrupt log record %lu", recnum);
		}

        // check if this bogus record is in the midst of a transaction
	    // (try to find a CloseTransaction log record)
        const unsigned long maxfollow = 3;
        dprintf(D_ALWAYS, "Lines following corrupt log record %lu (up to %lu):\n", recnum, maxfollow);
        unsigned long nlines = 0;
		while( fgets( line, ATTRLIST_MAX_EXPRESSION+64, fp ) ) {
            nlines += 1;
            if (nlines <= maxfollow) {
                dprintf(D_ALWAYS, "    %s", line);
                int ll = strlen(line);
                if (ll <= 0  ||  line[ll-1] != '\n') dprintf(D_ALWAYS, "\n");
            }
			if (sscanf( line, "%d ", &op ) != 1  ||  !valid_record_optype(op)) {
				// no op field in line; more bad log records...
				continue;
			}
			if( op == CondorLogOp_EndTransaction ) {
					// aargh!  bad record in transaction.  abort!
				EXCEPT("Error: corrupt log record %lu (byte offset %lld) occurred inside closed transaction, recovery failed", recnum, pos);
			}
		}

		if( !feof( fp ) ) {
			EXCEPT("Error: failed recovering from corrupt log record %lu, errno=%d", recnum, errno);
		}

			// there wasn't an error in reading the file, and the bad log 
			// record wasn't bracketed by a CloseTransaction; ignore all
			// records starting from the bad record to the end-of-file, and
			// pretend that we hit the end-of-file.
		fseek( fp , 0, SEEK_END);
		return( NULL );
	}

		// record was good
	return log_rec;
}

void ClassAdLog::ListNewAdsInTransaction( std::list<std::string> &new_keys )
{
	if( !active_transaction ) {
		return;
	}

	active_transaction->InTransactionListKeysWithOpType( CondorLogOp_NewClassAd, new_keys );
}
