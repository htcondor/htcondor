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

#if !defined(_LIBQMGR_H)
#define _LIBQMGR_H

#include "condor_common.h"
#include "condor_io.h"
#include "proc.h"
#include "../condor_utils/CondorError.h"
#include "condor_classad.h"


typedef struct {
	bool dummy;
} Qmgr_connection;

typedef int (*scan_func)(ClassAd *ad);

typedef unsigned char SetAttributeFlags_t;
const SetAttributeFlags_t NONDURABLE = (1<<0); // do not fsync
	// NoAck tells the remote version of SetAttribute to not send back a
	// return code.  If the operation fails, the connection will be closed,
	// so failure will be detected in CommitTransaction().  This is useful
	// for improving performance when setting lots of attributes.
const SetAttributeFlags_t SetAttribute_NoAck = (1<<1);
const SetAttributeFlags_t SETDIRTY = (1<<2);
const SetAttributeFlags_t SHOULDLOG = (1<<3);

#define SHADOW_QMGMT_TIMEOUT 300

// QmgmtPeer* getQmgmtConnectionInfo();
// bool setQmgmtConnectionInfo(QmgmtPeer *peer);
void unsetQmgmtConnection();


int InitializeConnection(const char *, const char *);
int InitializeReadOnlyConnection(const char * );

/** Initiate connection to schedd job queue and begin transaction.
	@param qmgr_location can be the name or sinful string of a schedd or
	       NULL to connect to the local schedd
    @param timeout specifies the maximum time (in seconds) to wait for TCP
	       connection establishment
    @param read_only can be set to true to skip the potentially slow
	       authenticate step for connections which don't modify the queue
    @param effective_owner if not NULL, will call QmgmtSetEffectiveOwner()
	@param schedd_version_str Version of schedd if known (o.w. NULL).
	@return opaque Qmgr_connection structure
*/		 
Qmgr_connection *ConnectQ(const char *qmgr_location, int timeout=0, 
				bool read_only=false, CondorError* errstack=NULL,
				const char *effective_owner=NULL,
				char const *schedd_version_str=NULL);

/** Close the connection to the schedd job queue, and optionally commit
	the transaction.
	@param qmgr pointer to Qmgr_connection object returned by ConnectQ
	@param commit_transactions set to true to commit the transaction, 
	and false to abort the transaction.
	@return true if commit was successful; false if transaction was aborted
*/
bool DisconnectQ(Qmgr_connection *qmgr, bool commit_transactions=true);

/** Start a new job cluster.  This cluster becomes the
	active cluster, and jobs may only be submitted to this cluster.
	@return the new cluster id on success, < 0 on failure: -1 == "owner check failed"
    -2 == "MAX_JOBS_SUBMITTED exceeded", -3 == "cluster id collision"
*/
int NewCluster(void);

/** Signal the start of a new job description (a new job process).
	@param cluster_id cluster id of the active job cluster (from NewCluster())
	@return -1 on failure; the new proc id on success
*/
int NewProc( int cluster_id);

const int DESTROYPROC_SUCCESS_DELAY = 1; // DestoryProc succeeded. The job is still enqueued, but that's okay
const int DESTROYPROC_SUCCESS = 0; // DestroyProc succeeded
const int DESTROYPROC_ERROR = -1; // DestroyProc failed in a non-specific way
const int DESTROYPROC_EACCES = -2; // DestroyProc failed: wrong user or other access problem
const int DESTROYPROC_ENOENT = -3; // DestroyProc failed: cluster.proc doesn't exist.
/** Remove job with cluster_id and proc_id from the queue.  This is a
	low-level mechanism.  Normally, to remove jobs from the queue, set the
	job status to REMOVED and send a DEACTIVATE_CLAIM_FORCIBLY command to the schedd.
	@return: 0 or greater is success.  Specific results are DESTROYPROC_SUCCESS(0) = job removed and DESTROYPROC_SUCCESS_DELAY(1) = job not _yet_ removed, but "done".  Negative numbers indicate failure.  Specific failures are DESTROYPROC_ERROR(-1) = Unknown/non-specific error, DESTROYPROC_EACCESS(-2) = Owner failed, and DESTROYPROC_ENOENT(-3) = Job doesn't exist.
*/
int DestroyProc(int cluster_id, int proc_id);
/** Remove a cluster of jobs from the queue.
*/
int DestroyCluster(int cluster_id, const char *reason = NULL);
/** For all jobs in the queue for which constraint evaluates to true, set
	attr = value.  The value should be a valid ClassAd value (strings
	should be surrounded by quotes).
	@return -1 on failure; 0 on success
*/
int SetAttributeByConstraint(const char *constraint, const char *attr,
							 const char *value,
							 SetAttributeFlags_t flags=0);
/** For all jobs in the queue for which constraint evaluates to true, set
	attr = value.  The value should be a valid ClassAd value (strings
	should be surrounded by quotes).
	@return -1 on failure; 0 on success
*/
int SetAttributeIntByConstraint(const char *constraint, const char *attr,
							 int value,
							 SetAttributeFlags_t flags=0);
/** For all jobs in the queue for which constraint evaluates to true, set
	attr = value.  The value should be a valid ClassAd value (strings
	should be surrounded by quotes).
	@return -1 on failure; 0 on success
*/
int SetAttributeFloatByConstraint(const char *constraing, const char *attr,
							   float value,
							   SetAttributeFlags_t flags=0);
/** For all jobs in the queue for which constraint evaluates to true, set
	attr = value.  The value should be a valid ClassAd value (strings
	should be surrounded by quotes).
	@return -1 on failure; 0 on success
*/
int SetAttributeStringByConstraint(const char *constraint, const char *attr,
							     const char *value,
							     SetAttributeFlags_t flags=0);
/** Set attr = value for job with specified cluster and proc.  The value
	should be a valid ClassAd value (strings should be surrounded by
	quotes)
	@return -1 on failure; 0 on success
*/
int SetAttribute(int cluster, int proc, const char *attr, const char *value, SetAttributeFlags_t flags=0 );
/** Set attr = value for job with specified cluster and proc.  The value
	should be a valid ClassAd value (strings should be surrounded by
	quotes)
	@return -1 on failure; 0 on success
*/
int SetAttributeInt(int cluster, int proc, const char *attr, int value, SetAttributeFlags_t flags = 0 );
/** Set attr = value for job with specified cluster and proc.  The value
	should be a valid ClassAd value (strings should be surrounded by
	quotes)
	@return -1 on failure; 0 on success
*/
int SetAttributeFloat(int cluster, int proc, const char *attr, float value, SetAttributeFlags_t flags = 0);
/** Set attr = value for job with specified cluster and proc.  The value
	should be a valid ClassAd value (strings should be surrounded by
	quotes)
	@return -1 on failure; 0 on success
*/
int SetAttributeString(int cluster, int proc, const char *attr,
					   const char *value, SetAttributeFlags_t flags = 0);

/** Set LastJobLeaseRenewalReceived = <xact start time> and
    JobLeaseDurationReceived = dur for the specified cluster/proc.
	@return -1 on failure; 0 on success
*/
int SetTimerAttribute(int cluster, int proc, const char *attr_name, int dur);

/** Set the password to the MyProxy server for specified cluster/proc. The
	value should be a null-terminated string.
	@return -1 on failure; 0 on success
*/
int SetMyProxyPassword (int cluster, int proc, const char * pwd);


/** Tell the schedd that we're about to close the network socket. This
	call will not commit an active transaction. Callers of DisconnectQ()
	should not use this call.
	@return -1 on failure; 0 on success
*/
int CloseSocket();

bool InTransaction();
/** Begin a new transaction over an existing network connection to the
	schedd.
	@return -1 on failurer; 0 on success
*/
void BeginTransaction();

/** This didn't exist as a remote qmgmt call until 7.5.2.  Prior to that,
    the poorly named CloseConnection() call was used.
	@return -1 on failure: 0 on success
*/
int RemoteCommitTransaction(SetAttributeFlags_t flags=0);

/** The difference between this and RemoteCommitTransaction is that
	this function never returns if there is a failure.  This function
	should only be called from the schedd.
*/
void CommitTransaction(SetAttributeFlags_t flags=0);

void AbortTransaction();
void AbortTransactionAndRecomputeClusters();


void AbortTransaction();

/** Get value of attr for job with specified cluster and proc.
	@return -1 on failure; 0 on success
*/
int GetAttributeFloat(int cluster, int proc, const char *attr, float *value);
/** Get value of attr for job with specified cluster and proc.
	@return -1 on failure; 0 on success
*/
int GetAttributeInt(int cluster, int proc, const char *attr, int *value);
/** Get value of attr for job with specified cluster and proc.
	@return -1 on failure; 0 on success
*/
int GetAttributeBool(int cluster, int proc, const char *attr, int *value);
/** Get value of string attr for job with specified cluster and proc.
	@return -1 on failure; 0 on success. Allocates new copy of the string.
*/
int GetAttributeStringNew( int cluster_id, int proc_id, const char *attr_name, 
					   char **val );
/** Get value of string attr for job with specified cluster and proc.
	@return -1 on failure; 0 on success.
*/
int GetAttributeString( int cluster_id, int proc_id, char const *attr_name,
						MyString &val );
/** Get value of attr for job with specified cluster and proc.
	Allocates new copy of the unparsed expression string.
	@return -1 on failure; 0 on success
*/
int GetAttributeExprNew(int cluster, int proc, const char *attr, char **value);

/** Retrieves a classad of attributes that are marked as dirty, then clears
	the dirty list
*/
int GetDirtyAttributes(int cluster_id, int proc_id, ClassAd *updated_attrs);

/** Delete specified attribute for job with specified cluster and proc.
	@return -1 on failure; 0 on success
*/
int DeleteAttribute(int cluster, int proc, const char *attr);

/** Efficiently get the entire job ClassAd.
	The caller MUST call FreeJobAd when the ad is no longer in use. 
	@param cluster_id Cluster number of ad to fetch
	@param proc_id Process number of ad to fetch
	@param expStartdAttrs Expand $$(xxx) style macros inside the ClassAd
		with attributes from the matching Startd ad.  
	@param persist_expansions Save information about $$ expansions so that
        they can be reconstructed after restart of the schedd.
	@return NULL on failure; the job ClassAd on success
*/
ClassAd *GetJobAd(int cluster_id, int proc_id, bool expStardAttrs = false, bool persist_expansions = true );

/** Efficiently get the first job ClassAd which matches the constraint.
	@return NULL on failure; the job ClassAd on success
*/
ClassAd *GetJobByConstraint(const char *constraint);
/** Efficiently get the all jobs ClassAd which matches the constraint.
*/
void GetAllJobsByConstraint(const char *constraint, const char *proj,ClassAdList &list);
/** Iterate over all jobs in the queue.
	The caller MUST call FreeJobAd when the ad is no longer in use. 
	@param initScan should be non-zero on first call to initialize the iterator
	@return NULL on failure or when done iterating; the job ClassAd on success
*/
ClassAd *GetNextJob(int initScan);
/** Iterate over jobs in the queue which match the specified constraint.
	The caller MUST call FreeJobAd when the ad is no longer in use. 
*/
ClassAd *GetNextJobByConstraint(const char *constraint, int initScan);
/** Iterate over jobs with dirty attributes in the queue which match the
	specified constraint.
	The caller MUST call FreeJobAd when the ad is no longer in use. 
*/
ClassAd *GetNextDirtyJobByConstraint(const char *constraint, int initScan);
/** De-allocate job ClassAd allocated by GetJobAd, GetJobAdByConstraint,
	GetNextJob, or GetNextJobByConstraint.
*/
void FreeJobAd(ClassAd *&ad);

/** Initiate transfer of job's initial checkpoint file (the executable).
	Follow with a call to SendSpoolFileBytes.
	@param filename Name of initial checkpoint file destination
	@return -1 on failure; 0 on success
*/
int SendSpoolFile(char const *filename);

/** Actually transfer the initial checkpoint file (the executable).
	@param filename Name of initial checkpoint file source.
*/
int SendSpoolFileBytes(char const *filename);

/** New way of initializing transfer of ICKPT file.
    @return -1 on failure
            0 if client should send file
            1 if client need not send file since it's already there
*/
int SendSpoolFileIfNeeded(ClassAd& ad);

/* This function is not reentrant!  Do not call it recursively. */
void WalkJobQueue(scan_func);

bool InWalkJobQueue();

void InitQmgmt();
void InitJobQueue(const char *job_queue_name,int max_historical_logs);
void CleanJobQueue();
bool setQSock( ReliSock* rsock );
void unsetQSock();
void MarkJobClean(PROC_ID job_id);
void MarkJobClean(int cluster_id, int proc_id);
void MarkJobClean(const char* job_id_str);

int rusage_to_float(const struct rusage &, double *, double *);
int float_to_rusage(double, double, struct rusage *);

bool Reschedule();

#define SetAttributeExpr(cl, pr, name, val) SetAttribute(cl, pr, name, val);
#define SetAttributeExprByConstraint(con, name, val) SetAttributeByConstraint(con, name, val);

/* Set the effective owner to use for authorizing subsequent qmgmt
   opperations. Setting to NULL or an empty string will reset the
   effective owner to the real authenticated owner.  Changing to
   owner names other than the authenticated owner is only allowed
   for queue super users.
   Returns 0 on success. */

int QmgmtSetEffectiveOwner(char const *owner);

/* Call this to begin iterating over jobs in the queue that match
   a constraint.
   Returns 0 on success, -1 on error*/
int GetAllJobsByConstraint_Start( char const *constraint, char const *projection);
/* Retrieve next job matching constraint specified in call to
   GetAllJobsByCostraint_Start().
   Returns 0 on success, -1 on error or no more ads (sets errno on error).
*/
int GetAllJobsByConstraint_Next( ClassAd &ad );
int GetJobQueuedCount();

#endif
