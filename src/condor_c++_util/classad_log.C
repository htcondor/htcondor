/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "classad_log.h"
#include "condor_debug.h"
#include "util_lib_proto.h"


// explicitly instantiate the HashTable template
template class HashTable<HashKey, ClassAd*>;
template class HashBucket<HashKey,ClassAd*>;

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


ClassAdLog::ClassAdLog() : table(1024, hashFunction)
{
	log_filename[0] = '\0';
	active_transaction = NULL;
	log_fd = -1;
}

ClassAdLog::ClassAdLog(const char *filename) : table(1024, hashFunction)
{
	strcpy(log_filename, filename);
	active_transaction = NULL;

	log_fd = open(log_filename, O_RDWR | O_CREAT, 0600);
	if (log_fd < 0) {
		EXCEPT("failed to open log %s, errno = %d", log_filename, errno);
	}

	// Read all of the log records
	LogRecord		*log_rec;
	while ((log_rec = ReadLogEntry(log_fd, InstantiateLogEntry)) != 0) {
		switch (log_rec->get_op_type()) {
		case CondorLogOp_BeginTransaction:
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
				active_transaction->Commit(-1, (void *)&table); // commit in memory only
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
		if (log_fd>=0) {
			if (log->Write(log_fd) < 0) {
				EXCEPT("write to %s failed, errno = %d", log_filename, errno);
			}
			if (fsync(log_fd) < 0) {
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
	int new_log_fd;

	dprintf(D_FULLDEBUG,"About to truncate log %s\n",log_filename);
	sprintf(tmp_log_filename, "%s.tmp", log_filename);
	new_log_fd = open(tmp_log_filename, O_RDWR | O_CREAT, 0600);
	if (new_log_fd < 0) {
		dprintf(D_ALWAYS, "failed to truncate log: open(%s) returns %d\n",
				tmp_log_filename, new_log_fd);
		return;
	}
	LogState(new_log_fd);
	close(log_fd);
	close(new_log_fd);	// avoid sharing violation on move
	if (rotate_file(tmp_log_filename, log_filename) < 0) {
		dprintf(D_ALWAYS, "failed to truncate job queue log!\n");
		return;
	}
	log_fd = open(log_filename, O_RDWR | O_APPEND, 0600);
	if (log_fd < 0) {
		dprintf(D_ALWAYS, "failed to open log in append mode: "
			"open(%s) returns %d\n", log_filename, log_fd);
		return;
	}
}

void
ClassAdLog::BeginTransaction()
{
	assert(!active_transaction);
	active_transaction = new Transaction();
	EmptyTransaction = true;
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
	if (!EmptyTransaction) {
		LogEndTransaction *log = new LogEndTransaction;
		active_transaction->AppendLog(log);
		active_transaction->Commit(log_fd, (void *)&table);
	}
	delete active_transaction;
	active_transaction = NULL;
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
	for (LogRecord *log = active_transaction->FirstEntry(); log; 
		 log = active_transaction->NextEntry(log)) 
	{
		switch (log->get_op_type()) {
		case CondorLogOp_NewClassAd: {
			char *lkey = ((LogNewClassAd *)log)->get_key();
			if (strcmp(lkey, key) == 0) {
				adexists = true;
			}
			free(lkey);
			break;
		}
		case CondorLogOp_DestroyClassAd: {
			char *lkey = ((LogDestroyClassAd *)log)->get_key();
			if (strcmp(lkey, key) == 0) {
				adexists = false;
			}
			free(lkey);
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
	bool AdDeleted=false, ValDeleted=false, ValFound=false;

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
				if (stricmp(lname, name) == 0) {
					if (ValFound) {
						free(val);
					}
					val = ((LogSetAttribute *)log)->get_value();
					ValFound = true;
					ValDeleted = false;
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
				if (stricmp(lname, name) == 0) {
					if (ValFound) {
						free(val);
					}
					ValFound = false;
					ValDeleted = true;
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

	if (AdDeleted || ValDeleted) return -1;
	if (ValFound) return 1;
	return 0;
}

void
ClassAdLog::LogState(int fd)
{
	LogRecord	*log=NULL;
	ClassAd		*ad=NULL;
	HashKey		hashval;
	char		key[_POSIX_PATH_MAX];
    string      my, target, attr_name, attr_val;
	ClassAdUnParser unp;
	void*		chain;

	table.startIterations();
	while(table.iterate(ad) == 1) {
		table.getCurrentKey(hashval);
		hashval.sprint(key);
        ad->EvaluateAttrString(ATTR_TARGET_TYPE, target);
        ad->EvaluateAttrString(ATTR_MY_TYPE, my);
 
		log = new LogNewClassAd(key, my.data(), target.data());
		if (log->Write(fd) < 0) {
			EXCEPT("write to %s failed, errno = %d", log_filename, errno);
		}
		delete log;
//		chain = ad->unchain();
//		ad->ResetName();
//		attr_name = ad->NextName();
//		while (attr_name) {
//			attr_val = NULL;
//			expr = ad->Lookup(attr_name);
//			if (expr) {
//				expr->RArg()->PrintToNewStr(&attr_val);
//				log = new LogSetAttribute(key, attr_name, attr_val );
		ClassAd::iterator adIter;
		for( adIter = ad->begin( ); adIter != ad->end( ); adIter++ ) {
			if( adIter->second ) {
				unp.Unparse( attr_val, adIter->second );
				log = new LogSetAttribute(key, adIter->first.c_str( ),
										  attr_val.c_str( ) );
				if (log->Write(fd) < 0) {
					EXCEPT("write to %s failed, errno = %d", log_filename,
						   errno);
				}
//				free(attr_val);
				delete log;
//				delete [] attr_name;
//				attr_name = ad->NextName();
			}
		}
			// ok, now that we're done writing out this ad, restore the chain
		//ad->RestoreChain(chain);
	}
	if (fsync(fd) < 0) {
		EXCEPT("fsync of %s failed, errno = %d", log_filename, errno);
	} 
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
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	ClassAd	*ad = new ClassAd();   
	ad->SetMyTypeName(mytype);
	ad->SetTargetTypeName(targettype);
	return table->insert(HashKey(key), ad);
}


int
LogNewClassAd::ReadBody(int fd)
{
	int rval, rval1;
	free(key);
	rval = readword(fd, key);
	if (rval < 0) return rval;
	free(mytype);
	rval1 = readword(fd, mytype);
	if (rval1 < 0) return rval1;
	rval += rval1;
	free(targettype);
	rval1 = readword(fd, targettype);
	if (rval1 < 0) return rval1;
	return rval + rval1;
}


int
LogNewClassAd::WriteBody(int fd)
{
	int rval, rval1;
	rval = write(fd, key, strlen(key));
	if (rval < 0) return rval;
	rval1 = write(fd, " ", 1);
	if (rval1 < 0) return rval1;
	rval += rval1;
	rval1 = write(fd, mytype, strlen(mytype));
	if (rval1 < 0) return rval1;
	rval += rval1;
	rval1 = write(fd, " ", 1);
	if (rval1 < 0) return rval1;
	rval += rval1;
	rval1 = write(fd, targettype, strlen(targettype));
	if (rval1 < 0) return rval1;
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
	delete ad;
	return table->remove(hkey);
}


int
LogDestroyClassAd::ReadBody(int fd)
{
	free(key);
	return readword(fd, key);
}


LogSetAttribute::LogSetAttribute(const char *k, const char *n, const char *val)
{
	op_type = CondorLogOp_SetAttribute;
	key = strdup(k);
	name = strdup(n);
	value = strdup(val);
}


LogSetAttribute::~LogSetAttribute()
{
	free(key);
	free(name);
	free(value);
}


int
LogSetAttribute::Play(void *data_structure)
{
	ClassAdHashTable *table = (ClassAdHashTable *)data_structure;
	int rval;
	ClassAd *ad;

	if (table->lookup(HashKey(key), ad) < 0)
		return -1;
	rval = ad->InsertAttr( string(name), value );

	return rval;
}


int
LogSetAttribute::WriteBody(int fd)
{
	int		rval, rval1, len;

	len = strlen(key);
	rval = write(fd, key, len);
	if (rval < len) {
		return -1;
	}
	rval1 = write(fd, " ", 1);
	if (rval1 < 1) {
		return -1;
	}
	rval1 += rval;
	len = strlen(name);
	rval = write(fd, name, len);
	if (rval < len) {
		return -1;
	}
	rval1 += rval;
	rval = write(fd, " ", 1);
	if (rval < 1) {
		return rval;
	}
	rval1 += rval;
	len = strlen(value);
	rval = write(fd, value, len);
	if (rval < len) {
		return -1;
	}
	return rval1 + rval;
}


int
LogSetAttribute::ReadBody(int fd)
{
	int rval, rval1;

	free(key);
	rval1 = readword(fd, key);
	if (rval1 < 0) {
		return rval1;
	}

	free(name);
	rval = readword(fd, name);
	if (rval < 0) {
		return rval;
	}
	rval1 += rval;

	free(value);
	rval = readline(fd, value);
	if (rval < 0) {
		return rval;
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
	ClassAd *ad;
	if (table->lookup(HashKey(key), ad) < 0)
		return -1;
	return ad->Delete(name);
}


int
LogDeleteAttribute::WriteBody(int fd)
{
	int		rval, rval1, len;

	len = strlen(key);
	rval = write(fd, key, len);
	if (rval < len) {
		return -1;
	}
	rval1 = write(fd, " ", 1);
	if (rval1 < 1) {
		return -1;
	}
	rval1 += rval;
	len = strlen(name);
	rval = write(fd, name, len);
	if (rval < len) {
		return -1;
	}
	return rval1 + rval;
}


int 
LogBeginTransaction::ReadBody( int fd )
{
	char 	ch;
	int		rval = read( fd, &ch, 1 );
	if( rval < 1 || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}


int 
LogEndTransaction::ReadBody( int fd )
{
	char 	ch;
	int		rval = read( fd, &ch, 1 );
	if( rval < 1 || ch != '\n' ) {
		return( -1 );
	}
	return( 1 );
}


int
LogDeleteAttribute::ReadBody(int fd)
{
	int rval, rval1;

	free(key);
	rval1 = readword(fd, key);
	if (rval1 < 0) {
		return rval1;
	}

	free(name);
	rval = readword(fd, name);
	if (rval < 0) {
		return rval;
	}
	return rval + rval1;
}


LogRecord	*
InstantiateLogEntry(int fd, int type)
{
	LogRecord	*log_rec;

	switch(type) {
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
	    default:
		    return 0;
			break;
	}

		// check if we got a bogus record indicating a bad log file
	if( log_rec->ReadBody(fd) < 0 ) {

			// check if this bogus record is in the midst of a transaction
			// (try to find a CloseTransaction log record)


		char	line[ATTRLIST_MAX_EXPRESSION + 64];
		FILE	*fp = fdopen( fd, "r" );
		int		op;

		delete log_rec;
		if( !fp ) {
			EXCEPT("Error: failed fdopen() when recovering corrpupt log file");
		}

		while( fgets( line, ATTRLIST_MAX_EXPRESSION+64, fp ) ) {
			if( sscanf( line, "%d ", &op ) != 1 ) {
					// no op field in line; more bad log records...
				continue;
			}
			if( op == CondorLogOp_EndTransaction ) {
					// aargh!  bad record in transaction.  abort!
				EXCEPT("Error: bad record with op=%d in corrupt logfile",type);
			}
		}

		if( !feof( fp ) ) {
			EXCEPT("Error: failed recovering from corrupt file, errno=%d",
				errno );
		}

			// there wasn't an error in reading the file, and the bad log 
			// record wasn't bracketed by a CloseTransaction; ignore all
			// records starting from the bad record to the end-of-file, and
			// pretend that we hit the end-of-file.
		fclose( fp );
		return( NULL );
	}

		// record was good
	return log_rec;
}
