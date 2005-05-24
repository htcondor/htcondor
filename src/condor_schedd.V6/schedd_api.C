#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "CondorError.h"
#include "MyString.h"
#include "internet.h"

#include "condor_ckpt_name.h"
#include "condor_config.h"

#include "schedd_api.h"


Job::Job(int clusterId, int jobId):
	declaredFiles(64, MyStringHash, rejectDuplicateKeys)
{
	this->clusterId = clusterId;
	this->jobId = jobId;
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

	spoolDirectory = gen_ckpt_name(Spool, clusterId, jobId, 0);

	if (Spool) {
		free(Spool);
		Spool = NULL;
	}

	struct stat stats;
	if (-1 == stat(spoolDirectory.GetCStr(), &stats)) {
		if (ENOENT == errno && spoolDirectory.Length() != 0) {
			if (-1 == mkdir(spoolDirectory.GetCStr(), 0777)) {
					// mkdir can return 17 = EEXIST (dirname exists)
					// or 2 = ENOENT (path not found)
				dprintf(D_FULLDEBUG,
						"ERROR: mkdir(%s) failed, errno: %d (%s)\n",
						spoolDirectory.GetCStr(),
						errno,
						strerror(errno));

				errstack.pushf("SOAP",
							   FAIL,
							   "Creation of spool directory '%s' failed, "
							   "reason: %s",
							   spoolDirectory.GetCStr(),
							   strerror(errno));
				return 1;
			} else {
				dprintf(D_FULLDEBUG,
						"mkdir(%s) succeeded.\n",
						spoolDirectory.GetCStr());
			}
		} else {
			dprintf(D_FULLDEBUG, "ERROR: stat(%s) errno: %d (%s)\n",
					spoolDirectory.GetCStr(),
					errno,
					strerror(errno));

			errstack.pushf("SOAP",
						   FAIL,
						   "stat(%s) failed, reason: %s",
						   spoolDirectory.GetCStr(),
						   strerror(errno));

			return 2;
		}
	} else {
		dprintf(D_FULLDEBUG,
				"WARNING: Job '%d.%d''s spool '%s' already exists.\n",
				clusterId,
				jobId,
				spoolDirectory.GetCStr());
	}

	return 0;
}

int
Job::abort(CondorError &errstack)
{
	MyString currentKey;
	JobFile jobFile;
	declaredFiles.startIterations();
	while (declaredFiles.iterate(currentKey, jobFile)) {
		close(jobFile.file);
		declaredFiles.remove(currentKey);
		remove(jobFile.name.GetCStr());
	}

	remove(spoolDirectory.GetCStr());

	return 0;
}

int
Job::getClusterID()
{
	return clusterId;
}

JobFile::JobFile()
{
}

JobFile::~JobFile()
{
}

FileInfo::FileInfo()
{
}

FileInfo::~FileInfo()
{
	if (this->name) {
		free(this->name);
		this->name = NULL;
	}
}

int
FileInfo::initialize(const char *name, unsigned long size)
{
	this->size = size;
	this->name = strdup(name);

	return NULL == this->name;
}

int
Job::get_spool_list(List<FileInfo> &file_list,
					CondorError &errstack)
{
	StatInfo directoryInfo(spoolDirectory.GetCStr());
	if (directoryInfo.IsDirectory()) {
		Directory directory(spoolDirectory.GetCStr());
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
				spoolDirectory.GetCStr());

		errstack.pushf("SOAP",
					   FAIL,
					   "spool directory '%s' is not actually a directory.",
					   spoolDirectory.GetCStr());

		return 1;
	}
}

int
Job::declare_file(const MyString &name,
                  int size,
				  CondorError &errstack)
{
	JobFile *ignored;
	JobFile jobFile;
	jobFile.size = size;
	jobFile.currentOffset = 0;

	jobFile.name = name;

	jobFile.file =
		open((spoolDirectory + DIR_DELIM_STRING + jobFile.name).GetCStr(),
			 O_WRONLY | O_CREAT | _O_BINARY,
			 0600);
	if (-1 != jobFile.file) {
		if (0 == declaredFiles.lookup(name, ignored)) {
			errstack.pushf("SOAP",
						   ALREADYEXISTS,
						   "File '%s' already declared.",
						   name.GetCStr());

			return 4;
		}

		if (declaredFiles.insert(name, jobFile)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to record file '%s'.",
						   name.GetCStr());

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
					(spoolDirectory+DIR_DELIM_STRING+jobFile.name).GetCStr(),
					strerror(errno));

			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to open '%s' for writing, reason: %s",
						   name.GetCStr(),
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
	rval = SetAttributeString(clusterId,
							  jobId,
							  ATTR_JOB_IWD,
							  spoolDirectory.GetCStr());
	if (rval < 0) {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to set job %d.%d's %s attribute to '%s'.",
					   clusterId,
					   jobId,
					   ATTR_JOB_IWD,
					   spoolDirectory.GetCStr());

		return rval;
	}

	StringList transferFiles;
	MyString currentKey;
	JobFile jobFile;
	declaredFiles.startIterations();
	while (declaredFiles.iterate(currentKey, jobFile)) {
		transferFiles.append(jobFile.name.GetCStr());
	}

	char *fileList = NULL;
	if (0 == transferFiles.number()) {
		fileList = strdup("");
	} else {
		fileList = transferFiles.print_to_string();
		ASSERT(fileList);
	}

	rval = SetAttributeString(clusterId,
							  jobId,
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
					   clusterId,
					   jobId,
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

			rval = SetAttributeString(clusterId, jobId, name, value);
		} else {
				// all other types can be deduced by the ClassAd parser
			rval = SetAttribute(clusterId,jobId,name,value);
		}
		if ( rval < 0 ) {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to set job %d.%d's %s attribute.",
					   clusterId,
					   jobId,
					   name);

			return rval;
		}
	}

		// Trust the client knows what it is doing if there is an Iwd.
	if (!found_iwd) {
			// We need to make sure the Iwd is rewritten so files
			// in the spool directory can be found.
		rval = SetAttributeString(clusterId,
								  jobId,
								  ATTR_JOB_IWD,
								  spoolDirectory.GetCStr());
		if (rval < 0) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to set %d.%d's %s attribute to '%s'.",
						   clusterId,
						   jobId,
						   ATTR_JOB_IWD,
						   spoolDirectory.GetCStr());

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
					   name.GetCStr());

		return 1;
	}

	if (-1 != jobFile.file) {
		if (-1 == lseek(jobFile.file, offset, SEEK_SET)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to lseek in file '%s', reason: %s",
						   name.GetCStr(),
						   strerror(errno));

			return 2;
		}
		if (data_length != full_write(jobFile.file, data, data_length)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to write to file '%s', reason: %s",
						   name.GetCStr(),
						   strerror(errno));

			return 3;
		}
	} else {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to open file '%s', it should not "
						   "contain any path separators.",
						   name.GetCStr());

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
	int file = open((spoolDirectory + DIR_DELIM_STRING + name).GetCStr(),
					O_RDONLY | _O_BINARY,
					0);

	if (-1 != file) {
		if (-1 == lseek(file, offset, SEEK_SET)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to lseek in file '%s', reason: %s",
						   name.GetCStr(),
						   strerror(errno));

			return 2;
		}
		if (length != full_read(file, data, sizeof(unsigned char) * length)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to read from file '%s', reason: %s",
						   name.GetCStr(),
						   strerror(errno));

			return 3;
		}
		if (-1 == close(file)) {
			errstack.pushf("SOAP",
						   FAIL,
						   "Failed to close file '%s', reason: %s",
						   name.GetCStr(),
						   strerror(errno));

			return 4;
		}
	} else {
		errstack.pushf("SOAP",
					   FAIL,
					   "Failed to open file '%s', reason: %s",
					   name.GetCStr(),
					   strerror(errno));

		return 1;
	}

	return 0;
}
