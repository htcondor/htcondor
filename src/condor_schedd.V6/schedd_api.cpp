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
#include "condor_classad.h"
#include "condor_daemon_core.h"

#if HAVE_EXT_GSOAP

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "CondorError.h"
#include "MyString.h"
#include "internet.h"

#include "spooled_job_files.h"
#include "condor_config.h"
#include "condor_open.h"

#include "schedd_api.h"

using namespace soap_schedd;

Job::Job(PROC_ID pro_id):
	declaredFiles(64, MyStringHash, rejectDuplicateKeys)
{
	this->id = pro_id;
}

Job::~Job()
{
		// XXX: Duplicate code with abort(), almost.
	MyString currentKey;
	JobFile jobFile;
	declaredFiles.startIterations();
	while (declaredFiles.iterate(currentKey, jobFile)) {
		close(jobFile.file);
		declaredFiles.remove(currentKey);
	}
}

int
Job::initialize(CondorError &errstack)
{
	char * Spool = param("SPOOL");
	ASSERT(Spool);

	char *ckpt_name = gen_ckpt_name(Spool, id.cluster, id.proc, 0);
	spoolDirectory = ckpt_name;
	free(ckpt_name); ckpt_name = NULL;

	if (Spool) {
		free(Spool);
		Spool = NULL;
	}

	struct stat stats;
	if (-1 == stat(spoolDirectory.Value(), &stats)) {
		if (ENOENT == errno && spoolDirectory.Length() != 0) {

				// We assume here that the job is not a standard universe
				// job.  Spooling works differently for standard universe.
				// Unfortunately, we might not know the job universe
				// yet, so standard universe is problematic with SOAP
				// (and always has been).

			if( !SpooledJobFiles::createJobSpoolDirectory_PRIV_CONDOR(id.cluster,id.proc,false) ) {
				errstack.pushf("SOAP",
							   FAIL,
							   "Creation of spool directory '%s' failed, "
							   "reason: %s",
							   spoolDirectory.Value(),
							   strerror(errno));
				return 1;
			} else {
				dprintf(D_FULLDEBUG,
						"mkdir(%s) succeeded.\n",
						spoolDirectory.Value());
			}
		} else {
			dprintf(D_FULLDEBUG, "ERROR: stat(%s) errno: %d (%s)\n",
					spoolDirectory.Value(),
					errno,
					strerror(errno));

			errstack.pushf("SOAP",
						   FAIL,
						   "stat(%s) failed, reason: %s",
						   spoolDirectory.Value(),
						   strerror(errno));

			return 2;
		}
	} else {
		dprintf(D_FULLDEBUG,
				"WARNING: Job '%d.%d''s spool '%s' already exists.\n",
				id.cluster,
				id.proc,
				spoolDirectory.Value());
	}

	return 0;
}

int
Job::abort(CondorError & /* errstack */)
{
	MyString currentKey;
	JobFile jobFile;
	declaredFiles.startIterations();
	while (declaredFiles.iterate(currentKey, jobFile)) {
		close(jobFile.file);
		declaredFiles.remove(currentKey);
		MSC_SUPPRESS_WARNING_FIXME(6031);
		remove(jobFile.name.Value());
	}

	MSC_SUPPRESS_WARNING_FIXME(6031);
	remove(spoolDirectory.Value());

	return 0;
}

int
Job::getClusterID()
{
	return id.cluster;
}

JobFile::JobFile()
{
	currentOffset = -1;
	file = -1;
	size = -1;
}

JobFile::~JobFile()
{
}

FileInfo::FileInfo()
{
	name = NULL;
	size = 0;
}

FileInfo::~FileInfo()
{
	if (this->name) {
		free(this->name);
		this->name = NULL;
	}
}

int
FileInfo::initialize(const char *fName, filesize_t fSize)
{
	this->size = fSize;
	this->name = strdup(fName);

	return NULL == this->name;
}

int
Job::get_spool_list(List<FileInfo> &file_list,
					CondorError &errstack)
{
	StatInfo directoryInfo(spoolDirectory.Value());
	if (directoryInfo.IsDirectory()) {
		Directory directory(spoolDirectory.Value());
		const char * name;
		FileInfo *info;
		while (NULL != (name = directory.Next())) {
			info = new FileInfo();
			info->initialize(name, directory.GetFileSize());
			ASSERT(info);

			if (!file_list.Append(info)) {
				errstack.pushf("SOAP",
							   FAIL,
							   "Error adding %s to file list.",
							   name);

				return 2;
			}
		}

		return 0;
	} else {
		dprintf(D_ALWAYS, "spoolDirectory == '%s'\n",
				spoolDirectory.Value());

		errstack.pushf("SOAP",
					   FAIL,
					   "spool directory '%s' is not actually a directory.",
					   spoolDirectory.Value());

		return 1;
	}
}

int
Job::declare_file(const MyString &name,
                  filesize_t size,
				  CondorError &errstack)
{
	JobFile *ignored;
	JobFile jobFile;
	jobFile.size = size;
	jobFile.currentOffset = 0;

	jobFile.name = name;

	jobFile.file =
		safe_open_wrapper_follow((spoolDirectory + DIR_DELIM_STRING + jobFile.name).Value(),
			 O_WRONLY | O_CREAT | _O_BINARY,
			 0600);
	if (-1 != jobFile.file) {
		if (0 == declaredFiles.lookup(name, ignored)) {
			close(jobFile.file);
			errstack.pushf("SOAP",
						   ALREADYEXISTS,
						   "File '%s' already declared.",
						   name.Value());

			return 4;
		}

		if (declaredFiles.insert(name, jobFile)) {
			close(jobFile.file);
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to record file '%s'.",
						   name.Value());

			return 2;
		}
	} else {
			// If there is a path delimiter in the name we assume that
			// the client knows what she is doing and will set a
			// proper Iwd later on. If there is no path delimiter we
			// have a problem.
		if (-1 != name.FindChar(DIR_DELIM_CHAR)) {
			dprintf(D_FULLDEBUG,
					"Failed to open '%s' for writing, reason: %s\n",
					(spoolDirectory+DIR_DELIM_STRING+jobFile.name).Value(),
					strerror(errno));

			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to open '%s' for writing, reason: %s",
						   name.Value(),
						   strerror(errno));

			return 3;
		}
	}

	return 0;
}

int
Job::submit(const struct condor__ClassAdStruct &jobAd,
			CondorError &errstack)
{
	int i, rval;

		// XXX: This is ugly, and only should happen when spooling,
		// i.e. not always with cedar.
	rval = SetAttributeString(id.cluster,
							  id.proc,
							  ATTR_JOB_IWD,
							  spoolDirectory.Value());
	if (rval < 0) {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to set job %d.%d's %s attribute to '%s'.",
					   id.cluster,
					   id.proc,
					   ATTR_JOB_IWD,
					   spoolDirectory.Value());

		return rval;
	}

	StringList transferFiles;
	MyString currentKey;
	JobFile jobFile;
	declaredFiles.startIterations();
	while (declaredFiles.iterate(currentKey, jobFile)) {
		transferFiles.append(jobFile.name.Value());
	}

	char *fileList = NULL;
	if (0 == transferFiles.number()) {
		fileList = strdup("");
	} else {
		fileList = transferFiles.print_to_string();
		ASSERT(fileList);
	}

	rval = SetAttributeString(id.cluster,
							  id.proc,
							  ATTR_TRANSFER_INPUT_FILES,
							  fileList);

	if (fileList) {
		free(fileList);
		fileList = NULL;
	}

	if (rval < 0) {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to set job %d.%d's %s attribute.",
					   id.cluster,
					   id.proc,
					   ATTR_TRANSFER_INPUT_FILES);

		return rval;
	}

	int found_iwd = 0;
	for (i = 0; i < jobAd.__size; i++) {
		const char* name = jobAd.__ptr[i].name;
		const char* value = jobAd.__ptr[i].value;
		if (!name) continue;
		if (!value) value="UNDEFINED";

			// XXX: This is a quick fix. If processing MyType or
			// TargetType they should be ignored. Ideally we could
			// convert the ClassAdStruct to a ClassAd and then iterate
			// the ClassAd.
		if (0 == strcmp(name, ATTR_MY_TYPE) ||
			0 == strcmp(name, ATTR_TARGET_TYPE)) {
			continue;
		}

		if ( jobAd.__ptr[i].type == STRING_ATTR ) {
				// string type - put value in quotes as hint for ClassAd parser

			found_iwd = found_iwd || !strcmp(name, ATTR_JOB_IWD);

			rval = SetAttributeString(id.cluster, id.proc, name, value);
		} else {
				// all other types can be deduced by the ClassAd parser
			rval = SetAttribute(id.cluster, id.proc, name, value);
		}
		if ( rval < 0 ) {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to set job %d.%d's %s attribute.",
					   id.cluster,
					   id.proc,
					   name);

			return rval;
		}
	}

		// Trust the client knows what it is doing if there is an Iwd.
	if (!found_iwd) {
			// We need to make sure the Iwd is rewritten so files
			// in the spool directory can be found.
		rval = SetAttributeString(id.cluster,
								  id.proc,
								  ATTR_JOB_IWD,
								  spoolDirectory.Value());
		if (rval < 0) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to set %d.%d's %s attribute to '%s'.",
						   id.cluster,
						   id.proc,
						   ATTR_JOB_IWD,
						   spoolDirectory.Value());

			return rval;
		}
	}

	return 0;
}

int
Job::put_file(const MyString &name,
			  int offset,
			  char * data,
			  int data_length,
			  CondorError &errstack)
{
	JobFile jobFile;
	if (-1 == declaredFiles.lookup(name, jobFile)) {
		errstack.pushf("SOAP",
					   FAIL,
					   "File '%s' has not been declared.",
					   name.Value());

		return 1;
	}

	if (-1 != jobFile.file) {
		if (-1 == lseek(jobFile.file, offset, SEEK_SET)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to lseek in file '%s', reason: %s",
						   name.Value(),
						   strerror(errno));

			return 2;
		}
		int result;
		if (data_length !=
			(result = full_write(jobFile.file, data, data_length))) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to write to from file '%s', wanted to write %d bytes but was only able to write %d",
						   name.Value(),
						   data_length,
						   result);

			return 3;
		}
	} else {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to open file '%s', it should not "
						   "contain any path separators.",
						   name.Value());

		return 5;
	}

	return 0;
}

int
Job::get_file(const MyString &name,
              int offset,
              int length,
              unsigned char *&data,
			  CondorError &errstack)
{
	int file = safe_open_wrapper_follow((spoolDirectory + DIR_DELIM_STRING + name).Value(),
					O_RDONLY | _O_BINARY,
					0);

	if (-1 != file) {
		if (-1 == lseek(file, offset, SEEK_SET)) {
			close(file);
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to lseek in file '%s', reason: %s",
						   name.Value(),
						   strerror(errno));

			return 2;
		}
		int result;
		if (-1 == 
			(result = full_read(file, data, sizeof(unsigned char) * length))) {
			close(file);
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to read from file '%s', wanted to "
						   "read %d bytes but received %d",
						   name.Value(),
						   length,
						   result);

			return 3;
		}
		if (-1 == close(file)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to close file '%s', reason: %s",
						   name.Value(),
						   strerror(errno));

			return 4;
		}
	} else {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to open file '%s', reason: %s",
					   name.Value(),
					   strerror(errno));

		return 1;
	}

	return 0;
}


ScheddTransactionManager::ScheddTransactionManager():
	transactions(16, hashFuncInt, rejectDuplicateKeys)
{
}

ScheddTransactionManager::~ScheddTransactionManager()
{
}

int
ScheddTransactionManager::createTransaction(const char *owner,
											int &id,
											ScheddTransaction *&transaction)
{
	static int count = 0;

	transaction = new ScheddTransaction(owner);

		// This ID should allow for 256 transactions per second and
		// only repeat every 2^24 seconds (~= 1/2 year).
	id = time(NULL);
	id = (id << 8) + count;
	count = (count + 1) % 256;

	return transactions.insert(id, transaction);
}

int
ScheddTransactionManager::getTransaction(int id, ScheddTransaction *&transaction)
{
	transaction = NULL;
	return transactions.lookup(id, transaction);
}

int
ScheddTransactionManager::destroyTransaction(int id)
{
	ScheddTransaction *transaction;
	if (-1 == this->getTransaction(id, transaction)) {
   		return -1; // Unknown id
	} else {
		if (-1 == this->transactions.remove(id)) {
			return -2; // Remove failed
		} else {
			delete transaction;

			return 0;
		}
	}

	return -3; // New error code from lookup or remove...
}


ScheddTransaction::ScheddTransaction(const char *trans_owner):
	jobs(32, hashFuncPROC_ID, rejectDuplicateKeys)
{
	this->owner = trans_owner ? strdup(trans_owner) : NULL;
	duration = 0;
	trans_timer_id = -1;
	qmgmt_state = NULL;
}

ScheddTransaction::~ScheddTransaction()
{
	PROC_ID currentKey;
	Job *job;
	jobs.startIterations();
	while (jobs.iterate(currentKey, job)) {
		delete job;
		jobs.remove(currentKey);
	}

	if (this->owner) {
		free(this->owner);
		this->owner = NULL;
	}
	if (qmgmt_state) {
		delete qmgmt_state;
		qmgmt_state = NULL;
	}
	if ( trans_timer_id != -1 ) {
		daemonCore->Cancel_Timer(trans_timer_id);
		trans_timer_id = -1;
	}
}

int
ScheddTransaction::begin()
{
		// XXX: This will need to return a transaction, which will be recorded
	BeginTransaction();
	
	return 0;
}

void
ScheddTransaction::abort()
{
		// XXX: This needs to take a transaction
	AbortTransactionAndRecomputeClusters();
}

int
ScheddTransaction::commit()
{
		// XXX: This will need to take a transaction
	CommitTransaction();

		// aboutToSpawnJobHandler() needs to be called before a job
		// can potentially run. For most job types, it's called just
		// before spawning the shadow or starter. For grid jobs, we
		// call it at the end of job submission.
		// If we don't call it at all, job files in the spool directory
		// will be owned by the condor user, and we'll get permission
		// errors when attempting to access them as the user.
	PROC_ID currentKey;
	Job *job;
	ClassAd *job_ad;
	int universe;
	jobs.startIterations();
	while (jobs.iterate(currentKey, job)) {
		job_ad = GetJobAd( currentKey.cluster, currentKey.proc );
		if ( job_ad->LookupInteger( ATTR_JOB_UNIVERSE, universe ) &&
			 universe == CONDOR_UNIVERSE_GRID ) {

			aboutToSpawnJobHandler( currentKey.cluster, currentKey.proc, NULL );
		}
	}

	return 0;
}

int
ScheddTransaction::newCluster(int &id)
{
		// XXX: Need a transaction...
	id = NewCluster();

	return (id < 0) ? -1 : 0;
}

int
ScheddTransaction::newJob(int clusterId, int &id, CondorError & /* errstack */)
{
	id = NewProc(clusterId);
	if (-1 == id) {
		return -1;
	} else {
			// Create a Job for this new job.
		PROC_ID pid; pid.cluster = clusterId; pid.proc = id;
		Job *job = new Job(pid);
		ASSERT(job);
		CondorError error_stack;
		if (job->initialize(error_stack)) {
			delete job;
			return -2; // XXX: Cleanup? How do you undo NewProc?
		} else {
			if (this->jobs.insert(pid, job)) {
				return -3; // XXX: Cleanup? How do you undo NewProc?
			} else {
				return 0;
			}
		}
	}
}

int
ScheddTransaction::getJob(PROC_ID id, Job *&job)
{
	return jobs.lookup(id, job);
}

int
ScheddTransaction::removeJob(PROC_ID id)
{
	return jobs.remove(id);
}

int
ScheddTransaction::removeCluster(int clusterId)
{
	PROC_ID currentKey;
	Job *job;
	jobs.startIterations();
	while (jobs.iterate(currentKey, job)) {
		if (job->getClusterID() == clusterId) {
			if (jobs.remove(currentKey)) {
					// XXX: This is going to leave some jobs around, bad...
				return -1;
			}
		}
	}

	return 0;
}

int
ScheddTransaction::queryJobAds(const char *constraint, List<ClassAd> &ads)
{
		// XXX: Do this in a transaction (for ACID reasons)
	ClassAd *ad = GetNextJobByConstraint(constraint, 1);
	while (ad) {
		ads.Append(ad);
		ad = GetNextJobByConstraint(constraint, 0);
	}

	return 0;
}

int
ScheddTransaction::queryJobAd(PROC_ID id, ClassAd *&ad)
{
	ad = GetJobAd(id.cluster, id.proc);

	return 0;
}


const char *
ScheddTransaction::getOwner()
{
	return this->owner;
}

/*****************************************************************************
	   NullScheddTransaction, used when no ScheddTransaction is available...
*****************************************************************************/

NullScheddTransaction::NullScheddTransaction(const char * /* trans_owner */):
	ScheddTransaction(NULL)
{
}

NullScheddTransaction::~NullScheddTransaction() { }

int
NullScheddTransaction::begin()
{
	return -1;
}

void
NullScheddTransaction::abort() { }

int
NullScheddTransaction::commit()
{
	return -1;
}

int
NullScheddTransaction::newCluster(int & /* id */)
{
	return -2;
}

int
NullScheddTransaction::newJob(int /* clusterId */, int & /* id */, CondorError & /* errstack */)
{
	return -4;
}

int
NullScheddTransaction::getJob(PROC_ID /* id */, Job *& /* job */)
{
	return 0;
}

int
NullScheddTransaction::removeJob(PROC_ID /* id */)
{
	return 0;
}

int
NullScheddTransaction::removeCluster(int /* clusterId */)
{
	return 0;
}

#endif
