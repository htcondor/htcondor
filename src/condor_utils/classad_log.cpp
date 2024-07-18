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


#include "basename.h"
#include "condor_common.h"
#include "classad_log.h"
#include "condor_debug.h"
#include "util_lib_proto.h"
#include "classad_merge.h"
#include "condor_fsync.h"
#include "condor_attributes.h"

#if defined(UNIX)
#include "ClassAdLogPlugin.h"
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
/************************************************************/


const char *EMPTY_CLASSAD_TYPE_NAME = "(empty)";


// declare a default ClassAdLog table entry maker for the normal case
// when the log holds ClassAd's and not some derived type.
#ifdef WIN32
// windows (or probably not-using-shared-libaries?) requires this to be declared in a header file, not here
#else
// g++ (or using-shared-libaries?) requires this to be extern'ed in the header file and declared here.
const ConstructClassAdLogTableEntry<ClassAd*> DefaultMakeClassAdLogTableEntry;
#endif


// non-templatized worker function that implements the log loading functionality of ClassAdLog
//
FILE* LoadClassAdLog(
	const char *filename,
	LoggableClassAdTable & la,
	const ConstructLogEntry& maker,
	unsigned long & historical_sequence_number,
	time_t & m_original_log_birthdate,
	bool & is_clean,
	bool & requires_successful_cleaning,
	std::string & errmsg)
{
	FILE* log_fp = NULL;
	Transaction * active_transaction = NULL;

	historical_sequence_number = 1;
	m_original_log_birthdate = time(NULL);

	// TODO: this should open O_BINARY because on windows, text files have \r\n with the c-runtime magically adding/removing \r as needed.
	int log_fd = safe_open_wrapper_follow(filename, O_RDWR | O_CREAT | O_APPEND | O_LARGEFILE | _O_NOINHERIT, 0600);
	if (log_fd < 0) {
		formatstr(errmsg, "failed to open log %s, errno = %d\n", filename, errno);
		return NULL;
	}

	log_fp = fdopen(log_fd, "r+");
	if (log_fp == NULL) {
		formatstr(errmsg, "failed to fdopen log %s, errno = %d\n", filename, errno);
		return NULL;
	}

	is_clean = true; // was cleanly closed (until we find out otherwise)
	requires_successful_cleaning = false;

	// Read all of the log records
	LogRecord		*log_rec;
	unsigned long count = 0;
	long long next_log_entry_pos = 0;
    long long curr_log_entry_pos = 0;
	while ((log_rec = ReadLogEntry(log_fp, 1+count, InstantiateLogEntry, maker)) != 0) {
        curr_log_entry_pos = next_log_entry_pos;
		next_log_entry_pos = ftell(log_fp);
		count++;
		switch (log_rec->get_op_type()) {
		case CondorLogOp_Error:
			// this is defensive, ought to be caught in InstantiateLogEntry()
			formatstr(errmsg, "ERROR: in log %s transaction record %lu was bad (byte offset %lld)\n", filename, count, curr_log_entry_pos);
			fclose(log_fp); log_fp = NULL;

			delete active_transaction;
			return NULL;
			break;
		case CondorLogOp_BeginTransaction:
			// this file contains transactions, so it must not
			// have been cleanly shut down
			is_clean = false;
			if (active_transaction) {
				formatstr_cat(errmsg, "Warning: Encountered nested transactions, log may be bogus...\n");
			} else {
				active_transaction = new Transaction();
			}
			delete log_rec;
			break;
		case CondorLogOp_EndTransaction:
			if (!active_transaction) {
				formatstr_cat(errmsg, "Warning: Encountered unmatched end transaction, log may be bogus...\n");
			} else {
				active_transaction->Commit(NULL, NULL, &la); // commit in memory only
				delete active_transaction;
				active_transaction = NULL;
			}
			delete log_rec;
			break;
		case CondorLogOp_LogHistoricalSequenceNumber:
			if(count != 1) {
				formatstr_cat(errmsg, "Warning: Encountered historical sequence number after first log entry (entry number = %ld)\n",count);
			}
			historical_sequence_number = ((LogHistoricalSequenceNumber *)log_rec)->get_historical_sequence_number();
			m_original_log_birthdate = ((LogHistoricalSequenceNumber *)log_rec)->get_timestamp();
			delete log_rec;
			break;
		default:
			if (active_transaction) {
				active_transaction->AppendLog(log_rec);
			} else {
				log_rec->Play((void *)&la);
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
		formatstr_cat(errmsg, "Detected unterminated log entry\n");
		requires_successful_cleaning = true;
	}
	if (active_transaction) {	// abort incomplete transaction
		delete active_transaction;
		active_transaction = NULL;

		if( !requires_successful_cleaning ) {
			// For similar reasons as with broken log entries above,
			// we need to force rotation.
			formatstr_cat(errmsg, "Detected unterminated transaction\n");
			requires_successful_cleaning = true;
		}
	}
	if(!count) {
		log_rec = new LogHistoricalSequenceNumber( historical_sequence_number, m_original_log_birthdate );
		if (log_rec->Write(log_fp) < 0) {
			formatstr(errmsg, "write to %s failed, errno = %d\n", filename, errno);
			fclose(log_fp); log_fp = NULL;
		}
		delete log_rec;
	}

	return log_fp;
}


int FlushClassAdLog(FILE* fp, bool force)
{
	if ( ! fp)
		return 0;
	if (fflush(fp) !=0) {
		return errno ? errno : -1;
	}
	if (force) {
		// Then sync
		if (condor_fdatasync(fileno(fp)) < 0) {
			return errno ? errno : -1;
		}
	}
	return 0;
}


bool SaveHistoricalClassAdLogs(
	const char * filename,
	const unsigned long max_historical_logs,
	const unsigned long historical_sequence_number)
{
	if(!max_historical_logs) return true;

	std::string new_histfile;
	if(!formatstr(new_histfile,"%s.%lu",filename,historical_sequence_number))
	{
		dprintf(D_ALWAYS,"Aborting save of historical log: out of memory.\n");
		return false;
	}

	dprintf(D_FULLDEBUG,"About to save historical log %s\n",new_histfile.c_str());

	if( hardlink_or_copy_file(filename, new_histfile.c_str()) < 0) {
		dprintf(D_ALWAYS,"Failed to copy %s to %s.\n", filename, new_histfile.c_str());
		return false;
	}

	std::string old_histfile;
	if(!formatstr(old_histfile, "%s.%lu", filename, historical_sequence_number - max_historical_logs))
	{
		dprintf(D_ALWAYS,"Aborting cleanup of historical logs: out of memory.\n");
		return true; // this is not a fatal error
	}

	if( unlink(old_histfile.c_str()) == 0 ) {
		dprintf(D_FULLDEBUG,"Removed historical log %s.\n",old_histfile.c_str());
	}
	else {
		// It's ok if the old file simply doesn't exist.
		if( errno != ENOENT ) {
			// Otherwise, it's not a fatal error, but definitely odd that
			// we failed to remove it.
			dprintf(D_ALWAYS,"WARNING: failed to remove '%s': %s\n",old_histfile.c_str(),strerror(errno));
		}
		return true; // this is not a fatal error
	}
	return true;
}


bool TruncateClassAdLog(
	const char * filename,	        // in
	LoggableClassAdTable & la,      // in
	const ConstructLogEntry& maker, // in
	FILE* &log_fp,                  // in,out
	unsigned long & historical_sequence_number, // in,out
	time_t & m_original_log_birthdate, // in,out
	std::string & errmsg) // out
{
	std::string tmp_log_filename;
	int new_log_fd;
	FILE *new_log_fp;

	formatstr(tmp_log_filename, "%s.tmp", filename);
	new_log_fd = safe_create_replace_if_exists(tmp_log_filename.c_str(), O_RDWR | O_CREAT | O_LARGEFILE | _O_NOINHERIT, 0600);
	if (new_log_fd < 0) {
		formatstr(errmsg, "failed to rotate log: safe_create_replace_if_exists(%s) failed with errno %d (%s)\n",
			tmp_log_filename.c_str(), errno, strerror(errno));
		return false;
	}

	new_log_fp = fdopen(new_log_fd, "r+");
	if (new_log_fp == NULL) {
		formatstr(errmsg, "failed to rotate log: fdopen(%s) returns NULL\n",
				tmp_log_filename.c_str());
		close(new_log_fd);
		unlink(tmp_log_filename.c_str());
		return false;
	}


	// Now it is time to move courageously into the future.
	unsigned long future_sequence_number = historical_sequence_number + 1;

	// flush our current state into the temp file,
	// with a future value for sequence number
	bool success = WriteClassAdLogState(new_log_fp, tmp_log_filename.c_str(),
		future_sequence_number, m_original_log_birthdate,
		la, maker, errmsg);

	fclose(log_fp);
	log_fp = NULL;

	// we check for success in writing the new log AFTER we close the old one
	// so that any failure to write results in no log open. The looks a bit wrong
	// but it preserves the original behavior of ClassAdLog from when the worker
	// functions just EXCEPT'ed rather than returning errors.
	if ( ! success) {
		fclose(new_log_fp);
		unlink(tmp_log_filename.c_str());
		return false;
	}

	fclose(new_log_fp);	// avoid sharing violation on move
	if (rotate_file(tmp_log_filename.c_str(), filename) < 0) {
		formatstr(errmsg, "failed to rotate job queue log!\n");

		unlink(tmp_log_filename.c_str());

		int log_fd = safe_open_wrapper_follow(filename, O_RDWR | O_APPEND | O_LARGEFILE | _O_NOINHERIT, 0600);
		if (log_fd < 0) {
			formatstr(errmsg, "failed to reopen log %s, errno = %d after failing to rotate log.",filename,errno);
		} else {
			log_fp = fdopen(log_fd, "a+");
			if (log_fp == NULL) {
				formatstr(errmsg, "failed to refdopen log %s, errno = %d after failing to rotate log.",filename,errno);
				close(log_fd);
			}
		}

		return false;
	}

	// we successfully wrote and rotated, so we can update our sequence number
	historical_sequence_number = future_sequence_number;

#ifndef WIN32
	// POSIX does not provide any durability guarantees for rename().  Instead, we must
	// open the parent directory and invoke fsync there.
	std::string parent_dir = condor_dirname(filename);
	int parent_fd = safe_open_wrapper_follow(parent_dir.c_str(), O_RDONLY);
	if (parent_fd >= 0)
	{
		if (condor_fsync(parent_fd) == -1)
		{
			formatstr(errmsg, "Failed to fsync directory %s after rename. (errno=%d, msg=%s)", parent_dir.c_str(), errno, strerror(errno));
		}
		close(parent_fd);
	}
	else
	{
		formatstr(errmsg, "Failed to open parent directory %s for fsync after rename. (errno=%d, msg=%s)", parent_dir.c_str(), errno, strerror(errno));
	}
#endif

	int log_fd = safe_open_wrapper_follow(filename, O_RDWR | O_APPEND | O_LARGEFILE | _O_NOINHERIT, 0600);
	if (log_fd < 0) {
		formatstr(errmsg, "failed to open log in append mode: "
			"safe_open_wrapper(%s) returns %d", filename, log_fd);
	} else {
		log_fp = fdopen(log_fd, "a+");
		if (log_fp == NULL) {
			close(log_fd);
			formatstr(errmsg, "failed to fdopen log in append mode: "
				"fdopen(%s) returns %d", filename, log_fd);
		}
	}

	return true;
}


bool AddAttrNamesFromLogTransaction(
	Transaction* active_transaction,
	const char * key,
	classad::References & attrs) // out, attribute names are added when the transaction refers to them for the given key
{
	if ( !key ) {
		return false;
	}

		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return false;
	}

	int found = 0;

	for (LogRecord *log = active_transaction->FirstEntry(key); log;
		 log = active_transaction->NextEntry()) {
		switch (log->get_op_type()) {
		case CondorLogOp_SetAttribute: {
			char const *lname = ((LogSetAttribute *)log)->get_name();
			attrs.insert(lname);
			++found;
			break;
			}
		case CondorLogOp_DeleteAttribute: {
			char const *lname = ((LogDeleteAttribute *)log)->get_name();
			attrs.insert(lname);
			++found;
			break;
			}
		}
	}
	return found > 0;
}


int ExamineLogTransaction(
	Transaction* active_transaction,
	const ConstructLogEntry& maker, // in
	const char * key,
	const char *name,
	char *&val,
	ClassAd* &ad)
{
	bool AdDeleted=false, ValDeleted=false, ValFound=false;
	int attrsAdded = 0;


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
					ad = maker.New(((LogSetAttribute *)log)->get_key(), NULL);
					//PRAGMA_REMIND("tj: don't turn on dirty tracking here!")
					ad->EnableDirtyTracking();
					ASSERT(ad);
				}
				if (val) {
					free(val);
					val = NULL;
				}
				ExprTree* expr = ((LogSetAttribute *)log)->get_expr();
				if (expr) {
					expr = expr->Copy();
					ad->Insert(lname, expr);
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

bool AddAttrsFromLogTransaction(
	Transaction* active_transaction,
	const ConstructLogEntry& maker, // in
	const char * key,
	ClassAd &ad)
{
	if ( !key ) {
		return false;
	}

		// if there is no pending transaction, we're done
	if (!active_transaction) {
		return false;
	}

		// see what is going on in any current transaction
	char *val = NULL;
	ClassAd *attrsFromTransaction = NULL;
	ExamineLogTransaction(active_transaction, maker, key, NULL, val, attrsFromTransaction);
	if (attrsFromTransaction) {
		MergeClassAds(&ad,attrsFromTransaction,true);
		delete attrsFromTransaction;
		return true;
	}
	return false;
}


bool WriteClassAdLogState(
	FILE *fp, // in
	const char * filename,
	unsigned long historical_sequence_number, // in
	time_t m_original_log_birthdate, // in
	LoggableClassAdTable & la,
	const ConstructLogEntry& maker,
	std::string & errmsg)
{
	LogRecord	*log=NULL;
	ExprTree	*expr=NULL;

	// This must always be the first entry in the log.
	log = new LogHistoricalSequenceNumber( historical_sequence_number, m_original_log_birthdate );
	if (log->Write(fp) < 0) {
		formatstr(errmsg, "write to %s failed, errno = %d", filename, errno);
		delete log;
		return false;
	}
	delete log;

	const char * key;
	ClassAd * ad;

	la.startIterations();
	while(la.nextIteration(key, ad)) {
		log = new LogNewClassAd(key, GetMyTypeName(*ad), maker);
		if (log->Write(fp) < 0) {
			formatstr(errmsg, "write to %s failed, errno = %d", filename, errno);
			delete log;
			return false;
		}
		delete log;

			// Unchain the ad -- we just want to write out this ads exprs,
			// not all the exprs in the chained ad as well.
		classad::ClassAd *chain = ad->GetChainedParentAd();
		ad->Unchain();
		for ( auto itr = ad->begin(); itr != ad->end(); itr++ ) {
			expr = itr->second;
				// This conditional used to check whether the ExprTree is
				// invisible, but no codepath sets any attributes
				// invisible for this call.
			if (expr) {
				log = new LogSetAttribute(key, itr->first.c_str(),
										  ExprTreeToString(expr));
				if (log->Write(fp) < 0) {
					formatstr(errmsg, "write to %s failed, errno = %d", filename, errno);
					delete log;
					return false;
				}
				delete log;
			}
		}
			// ok, now that we're done writing out this ad, restore the chain
		ad->ChainToAd(chain);
	}
	if (fflush(fp) !=0){
		formatstr(errmsg, "fflush of %s failed, errno = %d", filename, errno);
	}
	if (condor_fdatasync(fileno(fp)) < 0) {
		formatstr(errmsg, "fsync of %s failed, errno = %d", filename, errno);
	}
	return true;
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
	YourStringDeserializer des(buf);
	des.deserialize_int(&historical_sequence_number);
	free(buf);

	rval1 = readword(fp, buf); //the label of the attribute 
				//we ignore it
	if (rval1 < 0) return rval1; 
	free(buf);

	rval1 = readword(fp, buf);
	if (rval1 < 0) return rval1;
	des = buf;
	des.deserialize_int(&timestamp);
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

LogNewClassAd::LogNewClassAd(const char *k, const char *m, const ConstructLogEntry & c) : ctor(c)
{
	op_type = CondorLogOp_NewClassAd;
	key = strdup(k);
	mytype = strdup(m);
}

LogNewClassAd::~LogNewClassAd()
{
	free(key);
	free(mytype);
}

int
LogNewClassAd::Play(void *data_structure)
{
	int result;
	LoggableClassAdTable *table = (LoggableClassAdTable *)data_structure;
	ClassAd *ad = ctor.New(key, mytype);
	SetMyTypeName(*ad, mytype);
	// backward compat hack for working with versions of HTCondor before 23.0
	if (mytype && MATCH == strcasecmp(mytype, "Job") && ! ad->Lookup(ATTR_TARGET_TYPE)) {
		ad->Assign(ATTR_TARGET_TYPE, "Machine");
	}
	ad->EnableDirtyTracking();
	result = table->insert(key, ad) ? 0 : -1;
	if ( result == -1 ) {
		ctor.Delete(ad);
	}

#if defined(UNIX)
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

	// Obsolete targettype, read and forget
	char * dummy = nullptr;
	rval1 = readword(fp, dummy);
	if (dummy) {
		free(dummy);
	} else {
		rval1 = 0; // not an error if targettype is missing.
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
	s = nullptr;
	// backward compatible creation of TargetType,
	// todo: remove this code once we no longer care about rolling back to HTCondor versions before 23.0
	if (mytype) {
		if (MATCH == strcasecmp(mytype,"Job")) {
			s = "Machine"; // old Schedd expects : Job Machine
		} else if (*mytype == '*') {
			s = mytype;    // old negotiator expects : * *
		}
	}
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

LogDestroyClassAd::LogDestroyClassAd(const char *k, const ConstructLogEntry & c) : ctor(c)
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
	LoggableClassAdTable *table = (LoggableClassAdTable *)data_structure;
	ClassAd *ad;

	if ( ! table->lookup(key, ad)) {
		return -1;
	}

#if defined(UNIX)
	ClassAdLogPluginManager::DestroyClassAd(key);
#endif

	ctor.Delete(ad);
	return table->remove(key) ? 0 : -1;
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
	value_expr = NULL;
	if (val && strlen(val) && !blankline(val) &&
		!ParseClassAdRvalExpr(val, value_expr))
	{
		value = strdup(val);
	} else {
		if (value_expr) delete value_expr;
		value_expr = NULL;
		value = strdup("UNDEFINED");
	}
	is_dirty = dirty;
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
	LoggableClassAdTable *table = (LoggableClassAdTable *)data_structure;
	int rval;
	ClassAd *ad = 0;
	if ( ! table->lookup(key, ad))
		return -1;

	std::string attr(name);
	if (ad->InsertViaCache(attr, value)) {
		rval = TRUE;
	} else {
		rval = FALSE;
	}
	if (is_dirty) {
		ad->MarkAttributeDirty(name);
	} else {
		ad->MarkAttributeClean(name);
	}

#if defined(UNIX)
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
			dprintf(D_ALWAYS, "WARNING: strict classad parsing failed for expression: %s\n", value);
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
	LoggableClassAdTable *table = (LoggableClassAdTable *)data_structure;
	ClassAd *ad = 0;
	if ( ! table->lookup(key, ad))
		return -1;

#if defined(UNIX)
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
LogBeginTransaction::Play(void *){
#if defined(UNIX)
	ClassAdLogPluginManager::BeginTransaction();
#endif

	return 1;
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
LogEndTransaction::Play(void *) {
#if defined(UNIX)
	ClassAdLogPluginManager::EndTransaction();
#endif

	return 1;
}


int
LogEndTransaction::WriteBody(FILE* fp)
{
	int rval = 0;
	if (comment) {
		int len = (int)strlen(comment);
		if (len > 0) {
			fputc('#', fp);
			rval = fwrite(comment, sizeof(char), len, fp);
			if (rval < len) {
				return -1;
			}
			rval += 1; // account for the # character
		}
	}
	return rval;
}

int 
LogEndTransaction::ReadBody( FILE* fp )
{
	char 	ch;
	int		rval = fread( &ch, sizeof(char), 1, fp );
	if( rval < 1 || (ch != '\n' && ch != '#') ) {
		return( -1 );
	}
	if (ch == '#') {
		int cch = readline(fp, comment);
		if (cch < 0)
			return -1;
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
InstantiateLogEntry(FILE *fp, unsigned long recnum, int type, const ConstructLogEntry & ctor)
{
	LogRecord	*log_rec;

	switch(type) {
        case CondorLogOp_Error:
            log_rec = new LogRecordError();
            break;
	    case CondorLogOp_NewClassAd:
		    log_rec = new LogNewClassAd("", "", ctor);
			break;
	    case CondorLogOp_DestroyClassAd:
		    log_rec = new LogDestroyClassAd("", ctor);
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
        dprintf(D_ALWAYS | D_ERROR, "WARNING: Encountered corrupt log record %lu (byte offset %lld)\n", recnum, pos);
		// TODO: this ugly code attempts to reconstruct the corrupted line, fix it to just show the actual line.
		const char *key, *name="", *value="";
		key = log_rec->get_key(); if ( ! key) key = "";
		if (log_rec->get_op_type() == CondorLogOp_SetAttribute) {
			LogSetAttribute * log = (LogSetAttribute *)log_rec;
			name = log->get_name(); if ( ! name) name = "";
			value = log->get_value(); if ( ! value) value = "";
		}
		dprintf(D_ALWAYS | D_ERROR, "    %d %s %s %s\n", log_rec->get_op_type(), key, name, value);

		char	line[ATTRLIST_MAX_EXPRESSION + 64];
		int		op;

		delete log_rec;

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

// Force instantiation of the simple form of ClassAdLog, used the the Accountant
//
template class ClassAdLog<std::string,ClassAd*>;
