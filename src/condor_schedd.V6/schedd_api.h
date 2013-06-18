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

#ifndef _schedd_api_h
#define _schedd_api_h

#include "condor_common.h"
#include "condor_classad.h"
#include "CondorError.h"
#include "MyString.h"
#include "HashTable.h"
#include "directory.h"
#include "list.h"
#include "qmgmt.h"

#ifdef HAVE_EXT_GSOAP
#include "soap_scheddH.h"
#endif

/**
   `A JobFile is a file that resides in a Job's spool. JobFile objects
   are used for writing to and reading from spool files.
 */
class JobFile
{
 public:
	JobFile();
	~JobFile();

		/// A file descriptor.
	int file;
		/// The file's name.
	MyString name;
		/// The file's size, in bytes.
	filesize_t size;
		/// The current offset in the file.
	filesize_t currentOffset;
};

/**
   A FileInfo object contains basic infomation about a file.
 */
class FileInfo
{
 public:
		/**
		   Construct a new FileInfo object. Be sure to call initialize()!
		 */
	FileInfo();
		/**
		   Free all memory allocated in the constructor.
		*/
	~FileInfo();

		/**
		   Initialize the FileInfo object. The name is strdup()ed and
		   free()d in the destructor.

		   @param name The name of the file.
		   @param size The number of bytes in the file.
		 */
	int initialize(const char *name, filesize_t size);

		/// The file's name.
	char *name;
		/// The size, in bytes, of the file.
	filesize_t size;
};

/**
   A Job, quite simply. It provides access its spool, including
   listing, reading and writing spool files. It also provides a
   mechanism for submission and aborting a submission.
 */
class Job
{
 public:
		/**
		   Construct a Job with the given cluster and job ids.

		   @param clusterId The cluster the Job is in.
		   @param jobId The Job's id.
		 */
	Job(PROC_ID id);

		/**
		   Destruct the Job. This does not abort the job or remove any
		   files. This does clean up all JobFiles, but does not remove
		   the files themselves. To remove files use abort().
		 */
	~Job();

		/**
		   Initialize the Job, which includes creating the Job's spool
		   directory.
		 */
	int initialize(CondorError &errstack);

		/**
		   Submit the Job. The submission process includes collecting
		   the declared files and adding them to the jobAd.

		   @param jobAd The Job's ClassAd.
		 */
	int submit(const struct soap_schedd::condor__ClassAdStruct &jobAd,
			   CondorError &errstack);

		/**
		   Declare a file in the Job's spool.

		   @param name The file's name.
		   @param size The file's size, in bytes.
		 */
	int declare_file(const MyString &name,
					 filesize_t size,
					 CondorError &errstack);

		/**
		   Put a file in the Job's spool. Actually, put a chunk of a
		   file's data into the file. Errors are reported if the file
		   was not declared with declare_file(), if the offset or
		   offset + data_length are outside the declared bounds of the
		   file.

		   @param name The file's name, identifing where to write data.
		   @param offset The place to seek to in the file for writing data.
		   @param data The data itself.
		   @param data_length The length of the data.
		 */
	int put_file(const MyString &name,
				 int offset,
				 char * data,
				 int data_length,
				 CondorError &errstack);

		/**
		   Get a piece of a file in the Job's spool. Errors happen
		   when the file does not exist, the offset is out of the
		   file's bounds, or the offset+length is out of the file's
		   bounds.

		   @param name The name of the spool file.
		   @param offset The location to seek to in the file.
		   @param length The amount of data to be read from the file.
		   @param data The resulting data.
		 */
	int get_file(const MyString &name,
				 int offset,
				 int length,
				 unsigned char * &data,
				 CondorError &errstack);

		/**
		   List the spool directory. This provides FileInfo for each
		   file in the Job's spool.
		 */
	int get_spool_list(List<FileInfo> &file_list,
					   CondorError &errstack);

		/**
		   Abort the Job's submission process. This will not terminate
		   the Job if it has already been submitted. This deletes all
		   memory associated with the Job and the Job's spool files.
		 */
	int abort(CondorError &errstack);

		/**
		   Get the cluster this Job lives in.
		 */
	int getClusterID();

 protected:
	PROC_ID id;
	HashTable<MyString, JobFile> declaredFiles;
	MyString spoolDirectory;
};

/**
   The ScheddTransaction is an interface providing operations that are
   to be performed transactionally within the Schedd. This class works
   as an abstraction over the normal qmgmt API. It assists in
   abstracting behavior out of the SOAP interface's implementation and
   into a form that can be used from Cedar. Construction of a
   ScheddTransaction should be done from the ScheddTransactionManager
   (a factory).

   This class does not implement all of the functionality in the qmgmt
   API, and some calls such as removeJob, which have qmgmt correlaries
   such as abortJob, behave only on the transaction's memory, not the
   actual queue. This is because operations in the qmgmt API cannot
   necessarily be rolled back on failure, such as abortJob (in schedd.C).
 */
class ScheddTransaction
{
		// Provide the ScheddTransactionManager with access to the
		// constructor and destructor.
	friend class ScheddTransactionManager;

public:
		/**
		   Begin a new transaction, starting the underlying
		   transaction. The value returned is 0 on success and non-0
		   otherwise.
		 */
	virtual int begin();

		/**
		   Abort the transaction. A ScheddTransaction should be
		   deleted and never used after an abort. Aborting a
		   transaction cannot fail (should not).
		 */
	virtual void abort();

		/**
		   Commit the transaction, making all operations performed in
		   the transaction lasting. The value returned is 0 on success
		   and non-0 otherwise.
		 */
	virtual int commit();

		/**
		   Create a new cluster. The return value is 0 on success and
		   non-0 otherwise.

		   @param id The new cluster's id, returned.
		 */
	virtual int newCluster(int &id);

		/**
		   Create a new job, must be within the context of an existing
		   cluster. The return value is 0 on success and non-0
		   otherwise.

		   @param clusterId The cluster (context) to house the new job
		   @param id The new job's id, returned
		   @param errstack A handy CondorError stack
		 */
	virtual int newJob(int clusterId, int &id, CondorError &errstack);

 		/**
		   Retrieve a job that has previously been created with
		   newJob. The return value is 0 on success and non-0
		   otherwise.

		   @param id The id of the job to return
		   @param job A pointer to the job
		 */
	virtual int getJob(PROC_ID id, Job *&job);

		/**
		   Remove a job from the transaction's memory, this does not
		   abort the job, for transactional reasons. To actually
		   remove the job from the queue use abortJob (qmgmt).

		   @param id The id of the job to remove
		 */
	virtual int removeJob(PROC_ID id);

		/**
		   Remove a cluster from the transaction's memory, this does
		   not abort the cluster's jobs, for transactional reasons. To
		   actually remove the jobs from the queue use abortJob
		   (qmgmt). The return value is 0 on success and non-0
		   otherwise.

		   @param id The id of the cluster to remove
		 */
	virtual int removeCluster(int clusterId);

		/**
		   Query the queue for a job, which is returned in the ad
		   parameter. The return value is 0 on success and non-0
		   otherwise.

		   @param id The id of the job to find in the queue
		   @param ad The job's ClassAd
		 */
	int queryJobAd(PROC_ID id, ClassAd *&ad);

		/**
		   Given a constrain find all jobs that match it in the
		   queue. The return value is 0 on success and non-0
		   otherwise.

		   @param constraint The constraint used for matching
		   @param ads The ClassAds that matched
		 */
	int queryJobAds(const char *constraint, List<ClassAd> &ads);

		/**
		   Get access to the transaction's owner. This call is provide
		   because ownership is thought to be an important aspect of
		   security for the transaction. All access control must be
		   performed by the user of the ScheddTransaction though 8o(
		 */
	const char *getOwner();

		// Duration of transaction.
	int duration;

		// DaemonCore Timer ID for a timer when the transaction expires
	int trans_timer_id;

		// All the qmgmt layer state
	QmgmtPeer* qmgmt_state;

protected:
		/**
		   Private constructor that makes a copy of the incoming
		   owner. Construction is performed by the
		   ScheddTransactionManager.

		   @param owner The transaction's owner
		*/
	ScheddTransaction(const char *owner);

	virtual ~ScheddTransaction();

	char *owner;
	HashTable<PROC_ID, Job *> jobs;
};

/**
   The ScheddTransactionManager is a factory for ScheddTransaction
   objects. It also provides a mapping between an externalizable
   transaction identifier and ScheddTransaction object.
 */
class ScheddTransactionManager
{
public:
	ScheddTransactionManager();
	~ScheddTransactionManager();

		/**
		   Create a new ScheddTransaction for the given owner and
		   return the transaction itself and its id, for later
		   lookup. The return value is 0 on success and non-0
		   otherwise.

		   @param owner The recorded owner of the transaction
		   @param id Returns the transaction's externalizable id
		   @param transaction Returns the newly created transaction
		 */
	int createTransaction(const char *owner,
						  int &id,
						  ScheddTransaction *&transaction);

		/**
		   Destroy a transaction. This does not abort or commit the
		   transaction. The return value is 0 on success and non-0
		   otherwise.

		   @param id The id of the transaction to destroy
		 */
	int destroyTransaction(int id);

		/**
		   Lookup a transaction by id. The return value is 0 on
		   success and non-0 otherwise.

		   @param id The id used for lookup
		   @param transaction Returned, the transaction with matching id
		 */
	int getTransaction(int id, ScheddTransaction *&transaction);

protected:
	HashTable<int, ScheddTransaction *> transactions;
};


/*****************************************************************************
	   NullScheddTransaction, used when no ScheddTransaction is available...
*****************************************************************************/

class NullScheddTransaction: protected ScheddTransaction
{
	friend bool stub_prefix(const char*, const soap*, int, int, DCpermission, bool, soap_schedd::condor__Transaction*&, soap_schedd::condor__Status&, ScheddTransaction*&);

public:
	virtual int begin();
	virtual void abort();
	virtual int commit();
	virtual int newCluster(int &id);
	virtual int newJob(int clusterId, int &id, CondorError &errstack);
	virtual int getJob(PROC_ID id, Job *&job);
	virtual int removeJob(PROC_ID id);
	virtual int removeCluster(int clusterId);

protected:
	NullScheddTransaction(const char *owner);

	virtual ~NullScheddTransaction();
};

#endif
