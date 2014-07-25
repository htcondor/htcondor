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
#include "file_transfer_db.h"
#include "condor_attributes.h"
#include "condor_constants.h"
#include "pgsqldatabase.h"
#include "basename.h"
#include "nullfile.h"
#include "internet.h"
#include "file_sql.h"
#include "subsystem_info.h"
#include "ipv6_hostname.h"
#include "my_hostname.h"


#ifdef HAVE_EXT_POSTGRESQL
extern FILESQL *FILEObj;

#define MAXSQLLEN 500
#define MAXMACHNAME 128

// Filenames are case sensitive on UNIX, but not Win32
#ifdef WIN32
#   define file_contains contains_anycase
#   define file_contains_withwildcard contains_anycase_withwildcard
#else
#   define file_contains contains
#   define file_contains_withwildcard contains_withwildcard
#endif


void file_transfer_db(file_transfer_record *rp, ClassAd *ad)
{
	MyString dst_host = "", 
		dst_path = "",
		globalJobId = "",
		src_name = "",
		src_path = "",
		iwd_path = "",
		job_name = "",
		dst_name = "",
		src_fullname = "";

	char *dynamic_buf = NULL;
	char buf[ATTRLIST_MAX_EXPRESSION];
	StringList *InputFiles = NULL;

	char src_host[MAXMACHNAME];
	bool inStarter  = FALSE;
		//char *tmpp;
	char *dir_tmp;

	struct stat file_status;

	ClassAd tmpCl1;
	ClassAd *tmpClP1 = &tmpCl1;
	MyString tmp;

	int dst_port = 0;
	int src_port = 0;
	MyString isEncrypted = "";

		// this function access the following pointers
	if  (!rp || !ad || !FILEObj)
		return;

		// check if we are in starter process
	if (get_mySubSystem()->isType(SUBSYSTEM_TYPE_STARTER) )
		inStarter = TRUE;

	ad->LookupString(ATTR_GLOBAL_JOB_ID, globalJobId);

		// dst_host, dst_name and dst_path, since file_transfer_db
		// is called in the destination process, dst_host is my
		// hostname
	dst_host = my_full_hostname();
	dst_name = condor_basename(rp->fullname);
	dir_tmp = condor_dirname(rp->fullname);
	dst_path = dir_tmp;
	free(dir_tmp);

		// src_host
	src_host[0] = '\0';
	if (rp->sockp) {
		MyString tmpp = get_hostname(rp->sockp->peer_addr());
		snprintf(src_host, MAXMACHNAME, "%s", tmpp.Value());
		dst_port = rp->sockp->get_port(); /* get_port retrieves the local port */
		src_port = rp->sockp->peer_port();
		isEncrypted = (!rp->sockp->get_encryption())?"FALSE":"TRUE";
	}

	bool found = false;
		// src_name, src_path
	if (inStarter && !dst_name.IsEmpty() &&
		(strcmp(dst_name.Value(), CONDOR_EXEC) == 0)) {
		ad->LookupString(ATTR_ORIG_JOB_CMD, job_name);
		if (!job_name.IsEmpty() && fullpath(job_name.Value())) {
			src_name = condor_basename(job_name.Value());
			dir_tmp = condor_dirname(job_name.Value());
			src_path = dir_tmp;
			free(dir_tmp);
			found = true;
		} else
			src_name = job_name;
		
	}
	else 
		src_name = dst_name;

	dynamic_buf = NULL;
	if (ad->LookupString(ATTR_TRANSFER_INPUT_FILES, &dynamic_buf) == 1) {
		InputFiles = new StringList(dynamic_buf,",");
		free(dynamic_buf);
		dynamic_buf = NULL;
	} else {
		InputFiles = new StringList(NULL,",");
	}
	if (ad->LookupString(ATTR_JOB_INPUT, buf, ATTRLIST_MAX_EXPRESSION) == 1) {
        // only add to list if not NULL_FILE (i.e. /dev/null)
        if ( ! nullFile(buf) ) {
            if ( !InputFiles->file_contains(buf) )
                InputFiles->append(buf);
        }
    }

	if (src_path.IsEmpty()) {
		if (inStarter)
			ad->LookupString(ATTR_ORIG_JOB_IWD, iwd_path);
		else 
			ad->LookupString(ATTR_JOB_IWD, iwd_path);
	}

	char *inputFile = NULL;

	InputFiles->rewind();
	while( !found && (inputFile = InputFiles->next()) ) {	
		const char *inputBaseName = condor_basename(inputFile);
		if(strcmp(inputBaseName, src_name.Value()) == 0) {
			found = true;
			if(fullpath(inputFile)) {
				dir_tmp = condor_dirname(inputFile);
				src_path = dir_tmp;
				free(dir_tmp);
			} else {
				src_path = iwd_path;
				char *inputDirName = condor_dirname(inputFile);
				// if dirname gives back '.', don't bother sticking it on
				if(!(inputDirName[0] == '.' && inputDirName[1] == '\0')) {
					src_path += DIR_DELIM_CHAR;
					src_path += inputDirName;
				}
				free(inputDirName);
			}	
		}
	}

	if(!found) {
		src_path = iwd_path;	
	}
	if(InputFiles) {
		delete InputFiles;
		InputFiles = NULL;
	}

	if (stat(rp->fullname, &file_status) < 0) {
		dprintf( D_ALWAYS, 
		"WARNING: File %s can not be accessed by Quill file transfer tracking.\n", rp->fullname);
	}
	tmpClP1->Assign("globalJobId", globalJobId);
	
	tmpClP1->Assign("src_name", src_name);
	tmpClP1->Assign("src_host", src_host);
	tmpClP1->Assign("src_port", src_port);
	tmpClP1->Assign("src_path", src_path);
	tmpClP1->Assign("dst_name", dst_name);
	tmpClP1->Assign("dst_host", dst_host);
	tmpClP1->Assign("dst_port", dst_port);
	tmpClP1->Assign("dst_path", dst_path);
	tmpClP1->Assign("transfer_size", (int)rp->bytes);
	tmpClP1->Assign("elapsed", (int)rp->elapsed);
	tmpClP1->Assign("dst_daemon", rp->daemon);
	tmpClP1->Assign("f_ts", (int)file_status.st_mtime);
	tmpClP1->Assign("transfer_time", (int)rp->transfer_time);
	tmpClP1->Assign("is_encrypted", isEncrypted);
	tmpClP1->Assign("delegation_method_id", rp->delegation_method_id);

	FILEObj->file_newEvent("Transfers", tmpClP1);

}
#endif
