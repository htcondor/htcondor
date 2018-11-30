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
#include "condor_uid.h"
#include "credmon_interface.h"
#include "directory.h"
#ifdef WIN32
#else
#include <fnmatch.h>
#endif


#define CREDMON_PID_FILE_READ_INTERVAL 20

static int _static_credmon_pid = -1;
static time_t _credmon_pid_timestamp = 0;

int get_credmon_pid() {
	if(_static_credmon_pid == -1 ||
	   time(NULL) > _credmon_pid_timestamp + CREDMON_PID_FILE_READ_INTERVAL) {

		// get pid of credmon
		MyString cred_dir;
		param(cred_dir, "SEC_CREDENTIAL_DIRECTORY");
		MyString pid_path;
		pid_path.formatstr("%s%cpid", cred_dir.c_str(), DIR_DELIM_CHAR);
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
		_credmon_pid_timestamp = time(NULL);
	}
	return _static_credmon_pid;
}


bool credmon_fill_watchfile_name(char* watchfilename, const char* user, const char* name = NULL) {

	// construct filename to poll for
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY"));
	if(!cred_dir) {
		dprintf(D_ALWAYS, "CREDMON: ERROR: got credmon_poll() but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return false;
	}

	if (!name) {
		name = "scitokens.use";
	}

	// if user == NULL this is a special case.  we want the credd to
	// refresh ALL credentials, which we know it has done when it writes
	// the file CREDMON_COMPLETE in the cred_dir
	if (user == NULL) {
		// we will watch for the file that signifies ALL creds were processed
		sprintf(watchfilename, "%s%cCREDMON_COMPLETE", cred_dir.ptr(), DIR_DELIM_CHAR);
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
		if(param_boolean("TOKENS", false)) {
			sprintf(watchfilename, "%s%c%s%c%s", cred_dir.ptr(), DIR_DELIM_CHAR, username, DIR_DELIM_CHAR, name);
		} else {
			sprintf(watchfilename, "%s%c%s.cc", cred_dir.ptr(), DIR_DELIM_CHAR, username);
		}
	}

	return true;
}


// takes a username, or NULL to refresh ALL credentials
// if force_fresh, we delete the file we are polling first
// if send_signal we SIGUP the credmon
// (we allow it, but you probably shouldn't force_fresh and not send_signal)
//
// returns true if those operations completed succesfully.
//
bool credmon_poll_setup(const char* user, bool force_fresh, bool send_signal) {

	// this will be the filename we poll for
	char watchfilename[PATH_MAX];
	if (credmon_fill_watchfile_name(watchfilename, user) == false) {
		return false;
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
#ifdef WIN32
		//TODO: send reconfig signal to credmon
		int rc = -1;
#else
		int rc = kill(credmon_pid, SIGHUP);
#endif
		if (rc == -1) {
			dprintf(D_ALWAYS, "CREDMON: failed to signal credmon: %i\n", errno);
			return false;
		}
	}
	return true;
}

// do exactly one test for the existance of the .cc file.  this does not block,
// just returns false right away and let's the caller decide what to do.
bool credmon_poll_continue(const char* user, int retry, const char* name) {

	// this will be the filename we poll for
	char watchfilename[PATH_MAX];
	if (credmon_fill_watchfile_name(watchfilename, user, name) == false) {
		return false;
	}

	struct stat junk_buf;

	// stat the file as root
	priv_state priv = set_root_priv();
	int rc = stat(watchfilename, &junk_buf);
	set_priv(priv);
	if (rc==-1) {
		dprintf(D_FULLDEBUG, "CREDMON: warning, got errno %i, waiting for %s to appear (retry: %i)\n", errno, watchfilename, retry);
		// DON'T BLOCK!  Just say we didn't find it and let the caller decide what to do.
		return false;
	}

	dprintf(D_FULLDEBUG, "CREDMON: SUCCESS: file %s found after %i seconds\n", watchfilename, 20-retry);
	return true;
}


// takes a username, or NULL to refresh ALL credentials
// if force_fresh, we delete the file we are polling first
// if send_signal we SIGUP the credmon
// (we allow it, but you probably shouldn't force_fresh and not send_signal)
//
// THIS FUNCTION MAY BLOCK!  if you need non-blocking, use the combination
// of credmon_poll_setup and credmon_poll_continue.
//
bool credmon_poll(const char* user, bool force_fresh, bool send_signal) {

	// this will be the filename we poll for
	char watchfilename[PATH_MAX];
	if (credmon_fill_watchfile_name(watchfilename, user) == false) {
		dprintf(D_ALWAYS, "CREDMON: FAILURE: unable to determine watchfile name for %s\n", user);
		return false;
	}

	// update files and send signals as needed
	if (!credmon_poll_setup(user, force_fresh, send_signal)) {
		return false;
	}

	// now poll repeatedly for existence of watch file
	int retries = 20;
	while (retries-- > 0) {
		if (credmon_poll_continue(user, retries)) {
			dprintf(D_FULLDEBUG, "CREDMON: SUCCESS: file %s found after %i seconds\n", watchfilename, 20-retries);
			return true;
		} else {
			sleep(1);
		}
	}

	dprintf(D_ALWAYS, "CREDMON: FAILURE: credmon never created %s after 20 seconds!\n", watchfilename);
	return false;
}



// takes a username, or NULL to refresh ALL credentials
// if force_fresh, we delete the file we are polling first
// if send_signal we SIGUP the credmon
// (we allow it, but you probably shouldn't force_fresh and not send_signal)
bool credmon_poll_obselete(const char* user, bool force_fresh, bool send_signal) {

	// construct filename to poll for
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY"));
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
		sprintf(watchfilename, "%s%cCREDMON_COMPLETE", cred_dir.ptr(), DIR_DELIM_CHAR);
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
		sprintf(watchfilename, "%s%c%s.cc", cred_dir.ptr(), DIR_DELIM_CHAR, username);
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
#ifdef WIN32
		//TODO: send reconfig signal to credmon
		int rc = -1;
#else
		int rc = kill(credmon_pid, SIGHUP);
#endif
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
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY"));
	if(!cred_dir) {
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
	sprintf(markfilename, "%s%c%s.mark", cred_dir.ptr(), DIR_DELIM_CHAR, username);

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

#ifdef WIN32
#else
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
#endif

void process_cred_mark_dir(const char *src) {
	auto_free_ptr cred_dir_name(param("SEC_CREDENTIAL_DIRECTORY"));

	// in theory, this should never be undefined if we got here, but we're about
	// to recursively delete directories as root so let's bail just to be safe.
	if(!cred_dir_name) {
		dprintf(D_ALWAYS, "CREDMON: SWEEPING, but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return;
	}

	Directory cred_dir(cred_dir_name.ptr(), PRIV_ROOT);

	dprintf (D_FULLDEBUG, "CREDMON: CRED_DIR: %s, MARK: %s\n", cred_dir_name.ptr(), src );
	if ( cred_dir.Find_Named_Entry( src ) ) {
		// it's possible that there are two users named "marky" and
		// "marky.mark".  make sure the marky.mark file is NOT a
		// directory, otherwise we'll delete poor marky's creds.
		if (cred_dir.IsDirectory()) {
			dprintf( D_ALWAYS, "SKIPPING DIRECTORY \"%s\" in %s\n", src, cred_dir_name.ptr());
			return;
		}
	} else {
		dprintf( D_ALWAYS, "CREDMON: Couldn't find dir \"%s\" in %s\n", src, cred_dir_name.ptr());
		return;
	}

	// delete the mark file (now that we're sure it's not a directory)
	dprintf( D_FULLDEBUG, "Removing %s%c%s\n", cred_dir_name.ptr(), DIR_DELIM_CHAR, src );
	if (!cred_dir.Remove_Current_File()) {
		dprintf( D_ALWAYS, "CREDMON: ERROR REMOVING %s%c%s\n", cred_dir_name.ptr(), DIR_DELIM_CHAR, src );
		return;
	}

	// delete the user's dir
	MyString username = src;
	username = username.substr(0, username.Length()-5);
	dprintf (D_FULLDEBUG, "CREDMON: CRED_DIR: %s, USERNAME: %s\n", cred_dir_name.ptr(), username.Value());
	if ( cred_dir.Find_Named_Entry( username.Value() ) ) {
		dprintf( D_FULLDEBUG, "Removing %s%c%s\n", cred_dir_name.ptr(), DIR_DELIM_CHAR, username.Value() );
		if (!cred_dir.Remove_Current_File()) {
			dprintf( D_ALWAYS, "CREDMON: ERROR REMOVING %s%c%s\n", cred_dir_name.ptr(), DIR_DELIM_CHAR, username.Value() );
			return;
		}
	} else {
		dprintf( D_ALWAYS, "CREDMON: Couldn't find dir \"%s\" in %s\n", username.Value(), cred_dir_name.ptr());
		return;
	}
}


void process_cred_mark_file(const char *src) {
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

	// construct filename to poll for
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY"));
	if(!cred_dir) {
		dprintf(D_FULLDEBUG, "CREDMON: skipping sweep, SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return;
	}

#ifdef WIN32
	// TODO: implement this.
#else
	MyString fullpathname;
	dprintf(D_FULLDEBUG, "CREDMON: scandir(%s)\n", cred_dir.ptr());
	struct dirent **namelist;
	int n = scandir(cred_dir, &namelist, &markfilter, alphasort);
	if (n >= 0) {
		while (n--) {
			if(param_boolean("TOKENS", false)) {
				process_cred_mark_dir(namelist[n]->d_name);
			} else {
				fullpathname.formatstr("%s%c%s", cred_dir.ptr(), DIR_DELIM_CHAR, namelist[n]->d_name);
				priv_state priv = set_root_priv();
				process_cred_mark_file(fullpathname.c_str());
				set_priv(priv);
			}
			free(namelist[n]);

		}
		free(namelist);
	} else {
		dprintf(D_FULLDEBUG, "CREDMON: skipping sweep, scandir(%s) got errno %i\n", cred_dir.ptr(), errno);
	}
#endif
}


bool credmon_clear_mark(const char* user) {
	auto_free_ptr cred_dir(param("SEC_CREDENTIAL_DIRECTORY"));
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
	sprintf(markfilename, "%s%c%s.mark", cred_dir.ptr(), DIR_DELIM_CHAR, username);

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
