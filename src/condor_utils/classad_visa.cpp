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
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "directory.h"
#include "classad_visa.h"
#include "ipv6_hostname.h"

bool
classad_visa_write(ClassAd* ad,
                   const char* daemon_type,
                   const char* daemon_sinful,
                   const char* dir_path,
                   std::string* filename_used)
{
	int cluster, proc, i;
	ClassAd visa_ad;
	std::string filename, fullpath;

	const char *file_path = NULL;
	int fd = -1;
	FILE *file = NULL;
	bool ret = false;

	if (ad == NULL) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: Ad is NULL\n");
		goto EXIT;
	}
	if (!ad->LookupInteger(ATTR_CLUSTER_ID, cluster)) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: Job contained no CLUSTER_ID\n");
		goto EXIT;
	}
	if (!ad->LookupInteger(ATTR_PROC_ID, proc)) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: Job contained no PROC_ID\n");
		goto EXIT;
	}

		// make a copy of the passed-in ad and tack on some attributes
		// describing this visa
	visa_ad = *ad;
	if (visa_ad.Assign(ATTR_VISA_TIMESTAMP, (int)time(NULL)) != TRUE) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: could not add attribute %s\n",
		        ATTR_VISA_TIMESTAMP);
		goto EXIT;
	}
	ASSERT(daemon_type != NULL);
	if (visa_ad.Assign(ATTR_VISA_DAEMON_TYPE, daemon_type) != TRUE) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: could not add attribute %s\n",
		        ATTR_VISA_DAEMON_TYPE);
		goto EXIT;
	}
	if (visa_ad.Assign(ATTR_VISA_DAEMON_PID, (int)getpid()) != TRUE) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: could not add attribute %s\n",
		        ATTR_VISA_DAEMON_PID);
		goto EXIT;
	}
	if (visa_ad.Assign(ATTR_VISA_HOSTNAME, get_local_fqdn()) != TRUE) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: could not add attribute %s\n",
		        ATTR_VISA_HOSTNAME);
		goto EXIT;
	}
	ASSERT(daemon_sinful != NULL);
	if (visa_ad.Assign(ATTR_VISA_IP, daemon_sinful) != TRUE) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: could not add attribute %s\n",
		        ATTR_VISA_IP);
		goto EXIT;
	}

		// Construct the file name to be: jobad.CLUSTER.PROC[.X],
		// where X is the lowest number possible to make the file name
		// unique, e.g. if jobad.0.0 exists then jobad.0.0.0 and if
		// jobad.0.0.0 exists then jobad.0.0.1 and so on
	i = 0;
	formatstr(filename, "jobad.%d.%d", cluster, proc);
	ASSERT(dir_path != NULL);
	file_path = dircat(dir_path, filename.c_str(), fullpath);
	while (-1 == (fd = safe_open_wrapper_follow(file_path,
	                                     O_WRONLY|O_CREAT|O_EXCL))) {
		if (EEXIST != errno) {
			dprintf(D_ALWAYS | D_FAILURE,
					"classad_visa_write ERROR: '%s', %d (%s)\n",
					file_path, errno, strerror(errno));
			goto EXIT;
		}

		formatstr(filename, "jobad.%d.%d.%d", cluster, proc, i++);
		file_path = dircat(dir_path, filename.c_str(), fullpath);
	}

	if (NULL == (file = fdopen(fd, "w"))) {
		dprintf(D_ALWAYS | D_FAILURE,
				"classad_visa_write ERROR: error %d (%s) opening file '%s'\n",
				errno, strerror(errno), file_path);
		goto EXIT;
	}
										   
	if (!fPrintAd(file, visa_ad)) {
		dprintf(D_ALWAYS | D_FAILURE,
		        "classad_visa_write ERROR: Error writing to file '%s'\n",
		        file_path);
		goto EXIT;
	}

	dprintf(D_FULLDEBUG,
			"classad_visa_write: Wrote Job Ad to '%s'\n",
			file_path);

	ret = true;

EXIT:
	if (file) {
		fclose(file);
	} else {
		if (-1 != fd) {
			close(fd);
		}
	}
	if (ret && (filename_used != NULL)) {
		*filename_used = filename;
	}
	return ret;
}
