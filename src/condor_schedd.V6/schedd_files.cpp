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
#include "condor_debug.h"
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

	tmp.formatstr("f_name = \"%s\"", fileName);
	tmpClP1->Insert(tmp.Value());		

	tmp.formatstr("f_host = \"%s\"", fs_domain);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("f_path = \"%s\"", path);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("f_ts = %d", (int)f_ts);
	tmpClP1->Insert(tmp.Value());	
		
	tmp.formatstr("f_size = %d", fsize);
	tmpClP1->Insert(tmp.Value());	

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

	tmp.formatstr("f_name = \"%s\"", fileName);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("f_host = \"%s\"", fs_domain);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("f_path = \"%s\"", path);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("f_ts = %d", (int) f_ts);
	tmpClP1->Insert(tmp.Value());	

	tmp.formatstr("globalJobId = \"%s\"", globalJobId);
	tmpClP1->Insert(tmp.Value());	
	
	tmp.formatstr("type = \"%s\"", type);
	tmpClP1->Insert(tmp.Value());	

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
	if (fullpath(tmpFile.Value())) {
		if (strcmp(tmpFile.Value(), "/dev/null") == 0)
			return; /* job doesn't care about this type of file */
		
		// condor_basename and condor_dirname may modify the string, so have two
		// copies
		pathname = tmpFile;
		MyString tmpFile2 = tmpFile;

		fileName = condor_basename(tmpFile.Value()); 
		char *dir_tmp = condor_dirname(tmpFile2.Value());
		path = dir_tmp;
		free(dir_tmp);
	}
	else {
		pathname.formatstr("%s/%s", path.Value(), tmpFile.Value());
		fileName = tmpFile;
	}

		// get the file status which contains timestamp and size
	if (stat(pathname.Value(), &file_status) < 0) {
		dprintf(D_FULLDEBUG, "ERROR: File '%s' can not be accessed.\n", 
				pathname.Value());
		fileExist = FALSE;
	}

	if(!fileExist) {
		return;
	}

		// insert the file entry into the files table
	retcode = schedd_files_ins_file(fileName.Value(), 
									fs_domain.Value(), 
									path.Value(), 
									file_status.st_mtime, 
									file_status.st_size);
	
	if (retcode == QUILL_FAILURE) {
			// fail to insert the file
		return;
	}

		// insert a usage for this file by this job
	schedd_files_ins_usage(globalJobId.Value(), type, 
						   fileName.Value(), 
						   fs_domain.Value(), 
						   path.Value(), 
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
