/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "classad_log.h"
#include "condor_debug.h"

static char *_FileName_ = __FILE__;	/* Used by EXCEPT (See condor_debug.h) */

// explicitly instantiate the HashTable template
template class HashTable<HashKey, ClassAd*>;
template class HashBucket<HashKey,ClassAd*>;

ClassAdLog::ClassAdLog() : table(1024, hashFunction)
{
	log_filename[0] = '\0';
	active_transaction = NULL;
	log_fp = NULL;
}

ClassAdLog::ClassAdLog(const char *filename) : table(1024, hashFunction)
{
	int fd;

	strcpy(log_filename, filename);
	active_transaction = NULL;

		// open the file with open to get the permissions right (in case the
		// the file is being created)  then wrap a FILE  around it
	if( ( fd = open(log_filename, O_RDWR | O_CREAT, 0600) ) < 0 ) {
		EXCEPT("failed to open log %s, errno = %d", log_filename, errno);
	}
	if( ( log_fp = fdopen( fd, "r+" ) ) == NULL ) {
		EXCEPT( "failed to fdopen() log %s, errno = %d", log_filename, errno );
	}

		// Read all of the log records
	LogRecord	*log_rec;
	while ((log_rec = ReadLogEntry(log_fp, InstantiateLogEntry)) != 0) {
		switch (log_rec->get_op_type()) {
		case CondorLogOp_BeginTransaction:
			if (active_transaction) {
				dprintf(D_ALWAYS, "Warning: Encountered nested transactions "
					"in %s, log may be bogus...", filename);
			} else {
				active_transaction = new Transaction();
			}
			delete log_rec;
			break;

		case CondorLogOp_EndTransaction:
			if (!active_transaction) {
				dprintf(D_ALWAYS, "Warning: Encountered unmatched end "
					"transaction in %s, log may be bogus...", filename);
			} else {
					// commit in memory only
				active_transaction->Commit(-1, (void *)&table); 
				delete active_transaction;
				active_transaction = NULL;
			}
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
	if (active_transaction) {	// abort incomplete transaction
		delete active_transaction;
		active_transaction = NULL;
	}
	TruncLog();
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
		if (EmptyTransaction) {
			LogBeginTransaction *log = new LogBeginTransaction;
			active_transaction->AppendLog(log);
			EmptyTransaction = false;
		}
		active_transaction->AppendLog(log);
	} else {
		if( log_fp ) {
			if (log->Write(log_fp) < 0) {
				EXCEPT("write to %s failed, errno = %d", log_filename, errno);
			}
			fflush( log_fp );
			if (fsync( fileno( log_fp ) ) < 0) {
				EXCEPT("fsync of %s failed, errno = %d", log_filename, errno);
			}
		}
		log->Play((void *)&table);
		delete log;
	}
}

void
ClassAdLog::TruncLog()
{
	char	tmp_log_filename[_POSIX_PATH_MAX];
	int 	new_log_fd;
	FILE	*new_log_fp;

	sprintf(tmp_log_filename, "%s.tmp", log_filename);
	new_log_fd = open(tmp_log_filename, O_RDWR | O_CREAT | O_TRUNC , 0600);
	if (new_log_fd < 0) {
		dprintf(D_ALWAYS, "failed to truncate log: open(%s) returns %d\n",
				tmp_log_filename, new_log_fd);
		return;
	}
	if( ( new_log_fp = fdopen( new_log_fd, "r+" ) ) == NULL ) {
		dprintf( D_ALWAYS, "failed to truncate log: fdopen(%s) failed, "
			"errno = %d\n", tmp_log_filename, errno );
		return;
	}

	LogState(new_log_fp);
	fclose(log_fp);
	fclose(new_log_fp);
#if defined(WIN32)
	if (MoveFileEx(tmp_log_filename, log_filename, MOVEFILE_REPLACE_EXISTING) 
			== 0) {
		dprintf(D_ALWAYS, "failed to truncate log: MoveFileEx failed with "
			"error %d\n", GetLastError());
		return;
	}
#else
	if (rename(tmp_log_filename, log_filename) < 0) {
		dprintf(D_ALWAYS, "failed to truncate log: rename(%s, %s) returns "
				"errno %d", tmp_log_filename, log_filename, errno);
		return;
	}
#endif
	if( ( log_fp = fopen( log_filename, "a+" ) ) == NULL ) {
		EXCEPT( "Failed to reopen %s, errno = %d", log_filename, errno );
	}
}

void
ClassAdLog::BeginTransaction()
{
	assert(!active_transaction);
	active_transaction = new Transaction();
	EmptyTransaction = true;
}

void
ClassAdLog::AbortTransaction()
{
	// Sometimes we do an AbortTransaction() when we don't know if there was
	// an active transaction.  This is allowed.
	if (active_transaction) {
		delete active_transaction;
		active_transaction = NULL;
	}
}

void
ClassAdLog::CommitTransaction()
{
	assert(active_transaction);
	if (!EmptyTransaction) {
		LogEndTransaction *log = new LogEndTransaction;
		active_transaction->AppendLog(log);
		active_transaction->Commit(log_fp, (void *)&table);
	}
	delete active_transaction;
	active_transaction = NULL;
}

int
ClassAdLog::LookupInTransaction(const char *key, const char *name, 
	ExprTree *&expr)
{
	bool AdDeleted=false, ExprDeleted=false, ExprFound=false;

	if (!active_transaction) return 0;

	for (LogRecord *log = active_transaction->FirstEntry(); log; 
		 log = active_transaction->NextEntry(log)) {

		switch (log->get_op_type()) {
		case CondorLogOp_NewClassAd: {
			if (AdDeleted) {	// check to see if ad is created after a delete
				char *lkey = ((LogNewClassAd *)log)->get_key();
				if (strcmp(lkey, key) == 0) {
					AdDeleted = false;
				}
				free(lkey);
			}
			break;
		}
		case CondorLogOp_DestroyClassAd: {
			char *lkey = ((LogDestroyClassAd *)log)->get_key();
			if (strcmp(lkey, key) == 0) {
				AdDeleted = true;
			}
			free(lkey);
			break;
		}
		case CondorLogOp_SetAttribute: {
			char *lkey = ((LogSetAttribute *)log)->get_key();
			if (strcmp(lkey, key) == 0) {
				char *lname = ((LogSetAttribute *)log)->get_name();
				if (strcmp(lname, name) == 0) {
					if (ExprFound) {
						if( expr ) delete expr;
					}
					expr = ((LogSetAttribute *)log)->get_expr();
					ExprFound = true;
					ExprDeleted = false;
				}
				free(lname);
			}
			free(lkey);
			break;
		}
		case CondorLogOp_DeleteAttribute: {
			char *lkey = ((LogDeleteAttribute *)log)->get_key();
			if (strcmp(lkey, key) == 0) {
				char *lname = ((LogDeleteAttribute *)log)->get_name();
				if (strcmp(lname, name) == 0) {
					if (ExprFound) {
						if( expr ) delete expr;
					}
					ExprFound = false;
					ExprDeleted = true;
				}
				free(lname);
			}
			free(lkey);
			break;
		}
		default:
			break;
		}
	}

	if (AdDeleted || ExprDeleted) return -1;
	if (ExprFound) return 1;
	return 0;
}

void
ClassAdLog::LogState( FILE *fp )
{
	LogRecord	*log=NULL;
	ClassAd		*ad=NULL;
	ExprTree	*expr=NULL;
	HashKey		hashval;
	char		key[_POSIX_PATH_MAX];
	char		*attr_name = NULL;
	ClassAdIterator	itor;

	table.startIterations();
	while(table.iterate(ad) == 1) {
		table.getCurrentKey(hashval);
		hashval.sprint(key);
		log = new LogNewClassAd(key);
		if (log->Write(fp) < 0) {
			EXCEPT("write to %s failed, errno = %d", log_filename, errno);
		}
		delete log;
		itor.Initialize( *ad );
		while( itor.NextAttribute( attr_name, expr ) ) {
			log = new LogSetAttribute(key, attr_name, expr);
			if (log->Write(fp) < 0) {
				EXCEPT("write to %s failed, errno = %d", log_filename, errno);
			}
			delete log;
		}
	}
	if (fsync(fileno(fp)) < 0) {
		EXCEPT("fsync of %s failed, errno = %d", log_filename, errno);
	} 
}

LogNewClassAd::LogNewClassAd(const char *k)
{
	op_type = CondorLogOp_NewClassAd;
	key = strdup(k);
}

LogNewClassAd::~LogNewClassAd()
{
	free(key);
}

int
LogNewClassAd::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	ClassAd	*ad = new ClassAd();
	return table->insert(HashKey(key), ad);
}


int
LogNewClassAd::ReadBody(FILE *fp)
{
	free(key);
	return( readword(fp, key) );
}


int
LogNewClassAd::WriteBody(FILE *fp)
{
	return( fprintf(fp, "%s", key) );
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
	delete ad;
	return table->remove(hkey);
}


int
LogDestroyClassAd::ReadBody(FILE *fp)
{
	free(key);
	return readword(fp, key);
}


LogSetAttribute::LogSetAttribute(const char *k, const char *n, ExprTree *tree)
{
	op_type = CondorLogOp_SetAttribute;
	key = strdup(k);
	name = strdup(n);
	expr = tree ? tree->Copy( ) : NULL;
}


LogSetAttribute::~LogSetAttribute()
{
	free(key);
	free(name);
	if( expr ) delete expr;
}


int
LogSetAttribute::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	ClassAd *ad;
	if (table->lookup(HashKey(key), ad) < 0)
		return -1;
	return( ad->Insert(name, expr->Copy( ) ) ? 1 : -1 );
}


int
LogSetAttribute::WriteBody(FILE *fp)
{
	int		rval;

	if( ( rval = fprintf( fp, "%s %s ", key, name ) ) < 0 ) return rval; 

		// the expression goes on the rest of the line; WriteTail puts \n
	Sink sink;
	sink.SetSink( fp );
	sink.SetTerminalChar( ' ' );
	if( !expr->ToSink( sink ) ) return -1; 
	sink.FlushSink( );

	return( rval + 1 );
}


int
LogSetAttribute::ReadBody(FILE *fp)
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

		// the rest of the line is an expression
	if( expr ) delete expr;
	Source src;
	src.SetSource( fp );
	src.SetSentinelChar( '\n' );
	if( !src.ParseExpression( expr ) ) return -1;

	return( rval1 + 1 );
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
	ClassAd *ad;
	if (table->lookup(HashKey(key), ad) < 0)
		return -1;
	return( ad->Delete(name) );
}


int
LogDeleteAttribute::WriteBody(FILE *fp)
{
	return( fprintf( fp, "%s %s", key, name ) );
}


int
LogDeleteAttribute::ReadBody(FILE *fp)
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
InstantiateLogEntry(FILE *fp, int type)
{
	LogRecord	*log_rec;

	switch(type) {
	    case CondorLogOp_NewClassAd:
		    log_rec = new LogNewClassAd("");
			break;
	    case CondorLogOp_DestroyClassAd:
		    log_rec = new LogDestroyClassAd("");
			break;
	    case CondorLogOp_SetAttribute:
		    log_rec = new LogSetAttribute("", "", NULL);
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
	    default:
		    return 0;
			break;
	}
	log_rec->ReadBody(fp);
	return log_rec;
}
