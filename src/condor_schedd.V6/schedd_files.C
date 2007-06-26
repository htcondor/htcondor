
#include "condor_common.h"
#include "schedd_files.h"
#include "condor_attributes.h"
#include "condor_md.h"
//#include <sys/mman.h>
#include "basename.h"
#include "my_hostname.h"
#include "file_sql.h"
#include "quill_enums.h"

extern FILESQL *FILEObj;

#define MAXSQLLEN 500
#define TIMELEN 30

QuillErrCode schedd_files_ins_file(
						  const char *fileName,
						  const char *fs_domain,
						  const char *path,
						  time_t f_ts,
						  int fsize)
{
	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp;

	tmp.sprintf("f_name = \"%s\"", fileName);
	tmpClP1->Insert(tmp.GetCStr());		

	tmp.sprintf("f_host = \"%s\"", fs_domain);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("f_path = \"%s\"", path);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("f_ts = %d", (int)f_ts);
	tmpClP1->Insert(tmp.GetCStr());	
		
	tmp.sprintf("f_size = %d", fsize);
	tmpClP1->Insert(tmp.GetCStr());	

	return FILEObj->file_newEvent("Files", tmpClP1);
}

void schedd_files_ins_usage(
							const char *globalJobId,
							const char *type,
							const char *fileName,
							const char *fs_domain,
							const char *path,
							time_t f_ts)
{
	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp;

	tmp.sprintf("f_name = \"%s\"", fileName);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("f_host = \"%s\"", fs_domain);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("f_path = \"%s\"", path);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("f_ts = %d", (int) f_ts);
	tmpClP1->Insert(tmp.GetCStr());	

	tmp.sprintf("globalJobId = \"%s\"", globalJobId);
	tmpClP1->Insert(tmp.GetCStr());	
	
	tmp.sprintf("type = \"%s\"", type);
	tmpClP1->Insert(tmp.GetCStr());	

	FILEObj->file_newEvent("Fileusages", tmpClP1);
}

void schedd_files_ins(
					  ClassAd *procad, 
					  const char *type)
{
	MyString fileName = "", tmpFile = "", fs_domain = "", path = "", 
		globalJobId = "";

	MyString pathname = "";

	struct stat file_status;
	bool fileExist = TRUE;
	QuillErrCode retcode;

		// get the file name (possibly with path) from classad
	procad->LookupString(type, tmpFile);

		// get host machine name from classad
	if (!procad->LookupString(ATTR_FILE_SYSTEM_DOMAIN, fs_domain)) {
			// use my hostname for files if fs_domain not found
		fs_domain = my_full_hostname();
	}
	
		// get current working directory
	procad->LookupString(ATTR_JOB_IWD, path);

	procad->LookupString(ATTR_GLOBAL_JOB_ID, globalJobId);

		// determine file name, path.
	if (fullpath(tmpFile.GetCStr())) {
		if (strcmp(tmpFile.GetCStr(), "/dev/null") == 0)
			return; /* job doesn't care about this type of file */
		
		// condor_basename and condor_dirname may modify the string, so have two
		// copies
		pathname = tmpFile;
		MyString tmpFile2 = tmpFile;

		fileName = condor_basename(tmpFile.GetCStr()); 
		path = condor_dirname(tmpFile2.GetCStr());
	}
	else {
		pathname.sprintf("%s/%s", path.GetCStr(), tmpFile.GetCStr());
		fileName = tmpFile;
	}

		// get the file status which contains timestamp and size
	if (stat(pathname.GetCStr(), &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR: File '%s' can not be accessed.\n", 
				pathname.GetCStr());
		fileExist = FALSE;
	}

	if(!fileExist) {
		return;
	}

		// insert the file entry into the files table
	retcode = schedd_files_ins_file(fileName.GetCStr(), 
									fs_domain.GetCStr(), 
									path.GetCStr(), 
									file_status.st_mtime, 
									file_status.st_size);
	
	if (retcode == QUILL_FAILURE) {
			// fail to insert the file
		return;
	}

		// insert a usage for this file by this job
	schedd_files_ins_usage(globalJobId.GetCStr(), type, 
						   fileName.GetCStr(), 
						   fs_domain.GetCStr(), 
						   path.GetCStr(), 
						   file_status.st_mtime);
}

void schedd_files(ClassAd *procad)

{
	FILEObj->file_lock();
	schedd_files_ins(procad, ATTR_JOB_CMD);	
	schedd_files_ins(procad, ATTR_JOB_INPUT);
	schedd_files_ins(procad, ATTR_JOB_OUTPUT);
	schedd_files_ins(procad, ATTR_JOB_ERROR);
	schedd_files_ins(procad, ATTR_ULOG_FILE);
	FILEObj->file_unlock();
}
