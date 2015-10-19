/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_config.h"


static int _static_credmon_pid = -1;

int get_credmon_pid() {
	if(_static_credmon_pid == -1) {
		// get pid of credmon
		MyString cred_dir;
		param(cred_dir, "SEC_CREDENTIAL_DIRECTORY");
		MyString pid_path;
		pid_path.formatstr("%s/pid", cred_dir.c_str());
		FILE* credmon_pidfile = fopen(pid_path.c_str(), "r");
		int num_items = fscanf(credmon_pidfile, "%i", &_static_credmon_pid);
		fclose(credmon_pidfile);
		if (num_items != 1) {
			_static_credmon_pid = -1;
		}
		dprintf(D_ALWAYS, "CERN: get_credmon_pid %s == %i\n", pid_path.c_str(), _static_credmon_pid);
	}
	return _static_credmon_pid;
}


bool credmon_signal_and_poll(const char* user) {

	// construct filename to poll for
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return false;
	}

	// get username (up to '@' if present, else whole thing)
	char username[256];
	const char *at = strchr(user, '@');
	if(at) {
		strncpy(username, user, (at-user));
		username[at-user] = 0;
	} else {
		strncpy(username, user, 255);
		username[255] = 0;
	}

	// check to see if .cc already exists
	char ccfilename[PATH_MAX];
	sprintf(ccfilename, "%s%c%s.cc", cred_dir, DIR_DELIM_CHAR, username);

	// now signal the credmon
	pid_t credmon_pid = get_credmon_pid();
	if (credmon_pid == -1) {
		dprintf(D_ALWAYS, "ZKM: failed to get pid of credmon.\n");
		return false;
	}

	dprintf(D_ALWAYS, "ZKM: sending SIGHUP to credmon pid %i\n", credmon_pid);
	int rc = kill(credmon_pid, SIGHUP);
	if (rc == -1) {
		dprintf(D_ALWAYS, "ZKM: failed to signal credmon: %i\n", errno);
		return false;
	}

	// now poll for existence of .cc file
	int retries = 20;
	struct stat junk_buf;
	while (retries > 0) {
		rc = stat(ccfilename, &junk_buf);
		if (rc==-1) {
			dprintf(D_ALWAYS, "ZKM: errno %i, waiting for %s to appear (%i seconds left)\n", errno, ccfilename, retries);
			sleep(1);
			retries--;
		} else {
			break;
		}
	}
	if (retries == 0) {
		dprintf(D_ALWAYS, "ZKM: FAILURE: credmon never created %s after 20 seconds!\n", ccfilename);
		return false;
	}

	dprintf(D_ALWAYS, "ZKM: SUCCESS: file %s found after %i seconds\n", ccfilename, 20-retries);
	return true;
}

