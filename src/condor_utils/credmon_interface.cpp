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


// none of this is intended for windows
#ifndef WINDOWS


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_uid.h"
#include <fnmatch.h>



static int _static_credmon_pid = -1;

int get_credmon_pid() {
	if(_static_credmon_pid == -1) {
		// get pid of credmon
		MyString cred_dir;
		param(cred_dir, "SEC_CREDENTIAL_DIRECTORY");
		MyString pid_path;
		pid_path.formatstr("%s/pid", cred_dir.c_str());
		FILE* credmon_pidfile = fopen(pid_path.c_str(), "r");
		if(!credmon_pidfile) {
			dprintf(D_FULLDEBUG, "CREDMON: unable to open %s (%i)\n", pid_path.c_str(), errno);
			return -1;
		}
		int num_items = fscanf(credmon_pidfile, "%i", &_static_credmon_pid);
		fclose(credmon_pidfile);
		if (num_items != 1) {
			dprintf(D_FULLDEBUG, "CREDMON: contents of %s unreadable\n", pid_path.c_str());
			_static_credmon_pid = -1;
			return -1;
		}
		dprintf(D_FULLDEBUG, "CREDMON: get_credmon_pid %s == %i\n", pid_path.c_str(), _static_credmon_pid);
	}
	return _static_credmon_pid;
}


// takes a username, or NULL to refresh ALL credentials
// if force_fresh, we delete the file we are polling first
// if send_signal we SIGUP the credmon
// (we allow it, but you probably shouldn't force_fresh and not send_signal)
bool credmon_poll(const char* user, bool force_fresh, bool send_signal) {

	// construct filename to poll for
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "CREDMON: ERROR: got credmon_poll() but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return false;
	}

	// this will be the filename we poll for
	char watchfilename[PATH_MAX];

	// if user == NULL this is a special case.  we want the credd to
	// refresh ALL credentials, which we know it has done when it writes
	// the file CREDMON_COMPLETE in the cred_dir
	if (user == NULL) {
		// we will watch for the file that signifies ALL creds were processed
		sprintf(watchfilename, "%s%cCREDMON_COMPLETE", cred_dir, DIR_DELIM_CHAR);
	} else {
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
		sprintf(watchfilename, "%s%c%s.cc", cred_dir, DIR_DELIM_CHAR, username);
	}

	if(force_fresh) {
		// unlink it first so we know we got a fresh copy
		priv_state priv = set_root_priv();
		unlink(watchfilename);
		set_priv(priv);
	}

	if(send_signal) {
		// now signal the credmon
		pid_t credmon_pid = get_credmon_pid();
		if (credmon_pid == -1) {
			dprintf(D_ALWAYS, "CREDMON: failed to get pid of credmon.\n");
			return false;
		}

		dprintf(D_FULLDEBUG, "CREDMON: sending SIGHUP to credmon pid %i\n", credmon_pid);
		int rc = kill(credmon_pid, SIGHUP);
		if (rc == -1) {
			dprintf(D_ALWAYS, "CREDMON: failed to signal credmon: %i\n", errno);
			return false;
		}
	}

	// now poll for existence of watch file
	int retries = 20;
	struct stat junk_buf;
	while (retries > 0) {
		int rc = stat(watchfilename, &junk_buf);
		if (rc==-1) {
			dprintf(D_FULLDEBUG, "CREDMON: warning, got errno %i, waiting for %s to appear (%i seconds left)\n", errno, watchfilename, retries);
			sleep(1);
			retries--;
		} else {
			break;
		}
	}
	if (retries == 0) {
		dprintf(D_ALWAYS, "CREDMON: FAILURE: credmon never created %s after 20 seconds!\n", watchfilename);
		return false;
	}

	dprintf(D_FULLDEBUG, "CREDMON: SUCCESS: file %s found after %i seconds\n", watchfilename, 20-retries);
	return true;
}

bool credmon_mark_creds_for_sweeping(const char* user) {

	// construct filename to create
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "CREDMON: ERROR: got mark_creds_for_sweeping but SEC_CREDENTIAL_DIRECTORY not defined!\n");
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
	char markfilename[PATH_MAX];
	sprintf(markfilename, "%s%c%s.mark", cred_dir, DIR_DELIM_CHAR, username);

	priv_state priv = set_root_priv();
	FILE* f = safe_fcreate_replace_if_exists(markfilename, "w", 0600);
	set_priv(priv);
	if (f == NULL) {
		dprintf(D_ALWAYS, "CREDMON: ERROR: safe_fcreate_replace_if_exists(%s) failed!\n", markfilename);
		return false;
	}

	fclose(f);
	return true;
}

// NOTE: some older platforms have a different signature for this function.
// I have added a "custom" cmake attribute called HAVE_OLD_SCANDIR which is
// currently set only for DARWIN 10.6 and 10.7.   -zmiller  2016-03-11
#ifdef HAVE_OLD_SCANDIR
int markfilter(dirent*d) {
#else
int markfilter(const dirent*d) {
#endif
  bool match = !fnmatch("*.mark", d->d_name, FNM_PATHNAME);
  // printf("d: %s, %i\n", d->d_name, match);
  return match;
}

void process_cred_file(const char *src) {
   //char * src = fname;
   char * trg = strdup(src);
   strcpy((trg + strlen(src) - 5), ".cred");
   dprintf(D_FULLDEBUG, "CREDMON: %li: FOUND %s UNLINK %s\n", time(0), src, trg);
   unlink(trg);
   strcpy((trg + strlen(src) - 5), ".cc");
   dprintf(D_FULLDEBUG, "CREDMON: %li: FOUND %s UNLINK %s\n", time(0), src, trg);
   unlink(trg);
   strcpy((trg + strlen(src) - 5), ".mark");
   dprintf(D_FULLDEBUG, "CREDMON: %li: FOUND %s UNLINK %s\n", time(0), src, trg);

   unlink(trg);

   free(trg);
}

void credmon_sweep_creds() {
	struct dirent **namelist;
	int n;

	// construct filename to poll for
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_FULLDEBUG, "CREDMON: skipping sweep, SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return;
	}

	MyString fullpathname;
	dprintf(D_FULLDEBUG, "CREDMON: scandir(%s)\n", cred_dir);
	n = scandir(cred_dir, &namelist, &markfilter, alphasort);
	if (n >= 0) {
		while (n--) {
			fullpathname.formatstr("%s%c%s", cred_dir, DIR_DELIM_CHAR, namelist[n]->d_name);
			priv_state priv = set_root_priv();
			process_cred_file(fullpathname.c_str());
			set_priv(priv);
			free(namelist[n]);
		}
		free(namelist);
	} else {
		dprintf(D_FULLDEBUG, "CREDMON: skipping sweep, scandir(%s) got errno %i\n", cred_dir, errno);
	}
	free(cred_dir);
}


bool credmon_clear_mark(const char* user) {
	char* cred_dir = param("SEC_CREDENTIAL_DIRECTORY");
	if(!cred_dir) {
		dprintf(D_ALWAYS, "CREDMON: ERROR: got credmon_clear_mark() but SEC_CREDENTIAL_DIRECTORY not defined!\n");
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

	// unlink the "mark" file on every update
	char markfilename[PATH_MAX];
	sprintf(markfilename, "%s%c%s.mark", cred_dir, DIR_DELIM_CHAR, username);

	priv_state priv = set_root_priv();
	int rc = unlink(markfilename);
	set_priv(priv);

	if(rc) {
		// ENOENT is common and not worth reporting as an error
		if(errno != ENOENT) {
			dprintf(D_FULLDEBUG, "CREDMON: warning! unlink(%s) got error %i (%s)\n",
				markfilename, errno, strerror(errno));
		}
	} else {
		dprintf(D_FULLDEBUG, "CREDMON: cleared mark file %s\n", markfilename);
	}

	return true;
}


#endif  // WINDOWS

