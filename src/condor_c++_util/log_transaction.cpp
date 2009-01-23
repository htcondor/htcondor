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
#include "condor_config.h"
#include "condor_mkstemp.h"

#define TRANSACTION_HASH_LEN 10000

/* Important parameters for local transaction backups follow.  

   The first of these is LOCAL_QUEUE_BACKUP_DIR.  The value of this
   parameter is a directory on the local filesystem in which we wish
   to store backups of individual transactions in the event of a
   failed write, flush, or sync to the job queue log on a shared
   filesystem. */

#define LOCAL_QUEUE_BACKUP_DIR "LOCAL_QUEUE_BACKUP_DIR"

/* The value of the LOCAL_XACT_BACKUP_FILTER parameter filters whether
   or not to backup any transactions based on whether or not the commit
   was successful.  Its value should be one of the following:
   
   "ALL", if we always want to keep local xact backups, no matter what,
   "NONE", if we should never keep local xact backups, or 
   "FAILED", if we should only keep backups of xacts that failed to commit
   
 */
#define LOCAL_XACT_BACKUP_FILTER "LOCAL_XACT_BACKUP_FILTER"

/* In the future, we might want to build in a way to select some
   transactions for backup independently of whether or not they
   complete, via some kind of expression (e.g. all transactions
   relating to a particular user or job).  Put another way, we might
   want to select some transactions based on one criterion, and of
   those, only back up the ones we've filtered out based on the
   LOCAL_XACT_BACKUP_FILTER. 

   This functionality isn't implemented at this time, but I'm making
   a note of it now.
*/
#define LOCAL_XACT_BACKUP_SELECT_EXPR "LOCAL_XACT_BACKUP_SELECT_EXPR"

enum 
backup_filter_t 
{
  BF_NONE, // if we should never keep local xact backups
  BF_ALL, // if we always want to keep local xact backups, no matter what
  BF_FAILED // if we should only keep backups of xacts that failed to commit
};

typedef struct {
  backup_filter_t filt;
  char* fn;		// filename
  FILE* fp;		// file pointer
  bool success;		// did we actually set up the backup?
} backup_info_t;


/* Used by stream_with_status_t */

enum 
stream_failure_t 
{ 
  WHY_OK,	/* no problems */
  WHY_WRITE,	/* write failed */
  WHY_FFLUSH,	/* fflush failed */
  WHY_FSYNC,	/* fsync failed */
  WHY_FCLOSE	/* fclose (or something called by it) failed */
};

/*
  stream_with_status_t keeps track of a stdio stream as well as
  whether or not one of the operations we've performed on it has
  failed (and, if so, the errno associated with it).
*/
typedef struct 
{
  FILE* fp;		/* a stream */
  stream_failure_t why;	/* a code indicating which call failed */
  int err;		/* the errno; we don't call it "errno"
			   because "errno" might be a macro */
} stream_with_status_t;


/* Frees any resources associated with a backup_info_t struct; does
   not free the actual struct */

static void 
cleanup_backup_info(backup_info_t* bi) 
{
  if (bi->fn) {
    free(bi->fn);
    bi->fn = NULL;
  }

  if (bi->fp) {
    fclose(bi->fp);
    bi->fp = NULL;
  }
}

static void 
remove_backup_file(backup_info_t* bi) 
{
  if (bi->fn != NULL) {
    // it's OK if this unlink fails, that's why we
    // don't check the return value....
    unlink(bi->fn); 
  }
}

/* Initializes a backup_info_t struct based on Condor parameters */
static void 
init_backup_info(backup_info_t* bi, bool skip_backup=false) 
{
  MyString local_file;
  int local_fd = -1;

  bi->filt = BF_NONE;
  bi->fn = NULL;
  bi->fp = NULL;
  bi->success = false;

  if (skip_backup) {
    return;
  }
  
  char* filter = param(LOCAL_XACT_BACKUP_FILTER);
  char* dir = param(LOCAL_QUEUE_BACKUP_DIR);

  if (filter == NULL || dir == NULL) {
    /* No parameter relating to backups was set; we can't store a
       backup in this case */

    goto cleanup;
  }

  if(strncasecmp("NONE", filter, strlen("NONE")) == 0) {
    /* The user explicitly requested no backups. */
    goto cleanup;
  }

  if(strncasecmp("ALL", filter, strlen("ALL")) == 0) {
    bi->filt = BF_ALL;
  } else if(strncasecmp("FAILED", filter, strlen("FAILED")) == 0) {
    bi->filt = BF_FAILED;
  } else {
    /* We're ignoring this value because we don't recognize it. */
    dprintf(D_ALWAYS, "Unknown %s value: %s\n", LOCAL_XACT_BACKUP_FILTER, filter);

    goto cleanup;
  }

  /* At this point, we know we *might* be needing a backup file */
  
  if (dir == NULL) {
    /* We can't keep backup log entries in this case */
    dprintf(D_ALWAYS, "You must specify a %s if you are going to specify a %s of %s", LOCAL_QUEUE_BACKUP_DIR, LOCAL_XACT_BACKUP_FILTER, filter);

    bi->filt = BF_NONE;
    goto cleanup;
  }
  
  local_file += dir;
  (local_file += DIR_DELIM_STRING) += "job_queue_log_backup_XXXXXX";
  
  bi->fn = local_file.StrDup();
  local_fd = condor_mkstemp(bi->fn);

  if(local_fd < 0) {
    /* We can't keep backup log entries if we don't have a backup file */
    bi->filt = BF_NONE;
    goto cleanup;
  }
  
  bi->fp = fdopen(local_fd, "w");
    
  /* 
     bi->success is only set to true if (1) we're here (meaning that
     we have set LOCAL_QUEUE_BACKUP_DIR and successfully created a temp
     file name), and (2) we have a non-null pointer for the backup file 
  */

  bi->success = (bi->fp != NULL);

 cleanup:
  /* The label is for cases of fast-fail -- we always want to free
     this memory on the way out of the function, though. */

  if (filter != NULL) {
    free(filter);
    filter = NULL;
  }
  
  if (dir != NULL) {
    free(dir);
    dir = NULL;
  }
}

static void 
init_stream_with_status(stream_with_status_t* s, FILE* fp) 
{
  ASSERT(s);

  s->fp = fp;
  s->why = WHY_OK;
  s->err = 0;
  return;
}

/*
  Wrapper for writing a log record to a stream pointer wrapped in a
  stream_with_status_t structure.  Returns 0 on success and -1 on
  failure.  In the event of a failure, it also sets the fp field of
  its argument to NULL (so that it will not be processed any further),
  the why field of its argument to WHY_WRITE, and the err field of its
  argument to errno.

  s must be non-null.
*/

static int 
write_with_status(LogRecord *log, stream_with_status_t* s) 
{
  ASSERT(s);

  if (s->fp == NULL || s->why != WHY_OK) {
    return 0; // short-circuiting in this case
  }

  if (log->Write(s->fp) < 0) {
    s->why = WHY_WRITE;
    s->err = errno;
    return -1;
  }
  
  return 0;
}

static int 
fclose_with_status(stream_with_status_t* s) 
{
  ASSERT(s);

  if (s->fp == NULL) {
    return 0;
  }

  if (fclose(s->fp) == EOF) {
    s->why = WHY_FCLOSE;
    s->err = errno;

    return -1;
  }

  /* We don't want to close this twice */
  s->fp = NULL;

  return 0;
}

/*
  Wrapper for flushing the buffers associated with a stream pointer
  wrapped in a stream_with_status_t structure.  Returns 0 on success
  and -1 on failure.  In the event of a failure, it also sets the fp
  field of its argument to NULL (so that it will not be processed any
  further), the why field of its argument to WHY_FFLUSH, and the err
  field of its argument to errno.

  s must be non-null.
*/
static int 
fflush_with_status(stream_with_status_t* s) 
{
  ASSERT(s);

  if (s->fp == NULL || s->why != WHY_OK) {
    return 0;
  }

  if(fflush(s->fp) != 0) {
    s->err = errno;
    s->why = WHY_FFLUSH;
    return -1;
  } 
  
  return 0;
}

/*
  Wrapper for syncing the file associated with a stream pointer
  wrapped in a stream_with_status_t structure.  Returns 0 on success
  and -1 on failure.  In the event of a failure, it also sets the fp
  field of its argument to NULL (so that it will not be processed any
  further), the why field of its argument to WHY_FSYNC, and the err
  field of its argument to errno.

  s must be non-null.
*/

static int
fsync_with_status(stream_with_status_t* s) 
{
  ASSERT(s);

  if (s->fp == NULL || s->why != WHY_OK) {
    return 0;
  }

  int fd = fileno(s->fp);
  if (fd >= 0) {

    /* XXX:  this maintains the same semantics as the original code --
       that is, not detecting a failure if s->fp has an invalid
       fileno.  Whether or not this is the right thing to do, it's at
       least backwards-compatible. */

	if (fsync(fd) < 0) {
	  s->why = WHY_FSYNC;
	  s->err = errno;
	  return -1;
	}
  }
  return 0;
}

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
	
	/*
	  We'd like to keep a backup of the job queue log on a local
	  filesystem in case writes, flushes, or syncs to the standard
	  job queue log fail.  Here's what we do:

	  1.  If there is a LOCAL_QUEUE_BACKUP_DIR parameter that
	  contains a valid directory name, then we will create a local
	  backup file in that directory with a unique name.  In this
	  case, we will assume that we might want a backup of the job
	  queue log.  (If not, we will not make a local backup;
	  proceed to #4.)
	  
	  2.  Every operation we do to the job queue log:  writes,
	  flushes, and syncs, we will also do to the local file.  We
	  will track the first error we see writing to the job queue
	  log and will stop writing to the actual job queue log -- but
	  not EXCEPT -- when we encounter the first error.

	  3.  Once we've completed writing log records, we will
	  unlink the local backup if there were no errors writing the
	  actual job queue log, *unless* the parameter
	  LOCAL_XACT_BACKUP_FILTER is set to "ALL".

	  4.  If there was a failure writing the actual job queue log,
	  we will then report (via EXCEPT) the first error we observed
	  while writing the actual job queue log, as well as the
	  location of the local backup file, if we made one.
	*/

	/* info about a backup:  do we keep one?  where is it?  etc. */
	backup_info_t bi;
	init_backup_info(&bi, (nondurable || fp == NULL));

	/* job queue log is element 0, backup file (if any) is
	   element 1 */
	stream_with_status_t fps[2];
	/* fps[0] will be the actual job queue log and fps[1] will be
	   the local backup.  If either file pointer is NULL, then
	   operations on that stream will be no-ops.  */
	init_stream_with_status(&(fps[0]), fp);
	init_stream_with_status(&(fps[1]), bi.fp);

	/* 
	   Do we want to keep local xact backups even if we
	   successfully wrote them to the actual log?
	*/
	bool fussy = bi.filt == BF_ALL;

	ordered_op_log.Rewind();

		// We're seeing sporadic test suite failures where 
		// CommitTransaction() appears to take a long time to
		// execute. Timing the I/O operations will help us
		// narrow down the cause
	time_t before, after;

	/*
	  We only go through the log once to avoid any interactions
	  with anything that happens to the log outside of this code
	  (or with the behavior of Rewind).
	 */

	while( (log = ordered_op_log.Next()) ) {
	  for (int i=0; i<2; i++) {
	    /* This call will only write to a file until it detects
	       the first error. */
	    before = time(NULL);
	    write_with_status(log, &(fps[i]));
		after = time(NULL);
		if ( (after - before) > 5 ) {
			dprintf( D_FULLDEBUG, "Transaction::Commit(): write_with_status() took %ld seconds to run\n", after - before );
		}
	  }

	  log->Play(data_structure);
	}

	if( !nondurable ) {
	  /* Note the change from the above:  we always want to flush
	     and sync the real job queue log, but we will only flush
	     and sync the local backup if we either (1) observed a
	     failure writing the log or (2) if we are keeping ALL
	     backups.

	     Here, we're operating on the real job queue log.
	  */
	  
	  before = time(NULL);
	  fflush_with_status(&(fps[0]));
	  after = time(NULL);
	  if ( (after - before) > 5 ) {
		  dprintf( D_FULLDEBUG, "Transaction::Commit(): fflush_with_status() took %ld seconds to run\n", after - before );
	  }

	  before = time(NULL);
	  fsync_with_status(&(fps[0]));
	  after = time(NULL);
	  if ( (after - before) > 5 ) {
		  dprintf( D_FULLDEBUG, "Transaction::Commit(): fsync_with_status() took %ld seconds to run\n", after - before );
	  }

	  bool failure = fps[0].why != WHY_OK;

	  if ((failure || fussy) && bi.filt != BF_NONE) {
	    /* 
	       Since syncs (especially) are expensive, only do these
	       operations on the local backup file if we're not going 
	       to be deleting it right away.
	    */

	    fflush_with_status(&(fps[1]));
	    fsync_with_status(&(fps[1]));

	    fclose_with_status(&(fps[1]));
	    bi.fp = NULL;

	    if(bi.success && fps[1].why == WHY_OK) {
	      dprintf(D_FULLDEBUG, "local backup of job queue log written to %s\n", bi.fn);
	    } else {
	      dprintf(D_ALWAYS, "FAILED to write local backup of job queue log to %s\n", bi.fn);
	    }
	  } else {
	    /* 
	       In this case, either our filter is BF_NONE, or we
	       successfully wrote the remote log.  NB:  if the filter
	       is BF_NONE, these two calls will be no-ops.
	    */
	    fclose_with_status(&(fps[1]));
	    bi.fp = NULL;
	    remove_backup_file(&bi);	    
	  }

	  if (failure) {
	    /*
	      Because we stop writing to the actual job queue log on
	      the first observed error, at most one of these will be
	      nonzero.
	    */
	    const char* why = "";

	    switch (fps[0].why) {
	    	case WHY_OK:
		  why = "nothing"; 
		  break;
	    	case WHY_WRITE: 
		  why = "write"; 
		  break;
	    	case WHY_FFLUSH: 
		  why = "fflush"; 
		  break;
	    	case WHY_FSYNC: 
		  why = "fsync"; 
		  break;
	    	case WHY_FCLOSE: 
		  why = "fclose"; 
		  break;
	    	default: 
		  why = "unknown";
	    }
	    
	    const char* made_backup = "";
		MyString backup_loc;

	    if (bi.filt != BF_NONE && bi.success && fps[1].why == WHY_OK) {
	      made_backup = "failed transaction logged to ";
	      backup_loc = bi.fn;
	    } else {
	      made_backup = "no local backup available.";
	    }
	    
	    cleanup_backup_info(&bi);
	    
	    EXCEPT("Failed to write real job queue log: %s failed (errno %d); %s%s", why, fps[0].err, made_backup, backup_loc.Value());
	  }
	  
	  cleanup_backup_info(&bi);
	  
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
