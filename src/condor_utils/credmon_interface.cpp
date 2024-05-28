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

// constuct filename <cred_dir>/<user><ext>
// if user ends in @domain, only part before the @ will be used
static const char * credmon_user_filename(std::string & file, const char * cred_dir, const char *user, const char * ext=NULL)
{
	dircat(cred_dir, user, file);

	// if username has a @ we need to remove that from the filename
	const char *at = strchr(user, '@');
	if (at) {
		size_t ix = file.find('@', strlen(cred_dir));
		file.erase(ix);
	}
	// append file extension (if any)
	if (ext) {
		file += ext;
	}
	return file.c_str();
}



#define CREDMON_PID_FILE_READ_INTERVAL 20

static int _static_credmon_pid = -1;
static time_t _credmon_pid_timestamp = 0;

int get_credmon_pid() {
	if(_static_credmon_pid == -1 ||
	   time(NULL) > _credmon_pid_timestamp + CREDMON_PID_FILE_READ_INTERVAL) {

		// get pid of credmon
		std::string cred_dir;
		param(cred_dir, "SEC_CREDENTIAL_DIRECTORY");
		std::string pid_path;
		formatstr(pid_path, "%s%cpid", cred_dir.c_str(), DIR_DELIM_CHAR);
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

//const int credmon_type_PWD = 0;
//const int credmon_type_KRB = 1;
//const int credmon_type_OAUTH = 2;
static const char * const credmon_type_names[] = { "Password", "Kerberos", "OAuth" };
static const char * credmon_type_name(int cred_type) {
	if (cred_type < 0 || cred_type >= (int)COUNTOF(credmon_type_names)) {
		return "!error";
	}
	return credmon_type_names[cred_type];
}

void credmon_clear_completion(int /*cred_type*/, const char * cred_dir)
{
	if (! cred_dir)
		return;

	std::string ccfile;
	dircat(cred_dir, "CREDMON_COMPLETE", ccfile);

	//TODO: the code in the master that was doing this before did not setpriv, should it?
	//priv_state priv = set_root_priv();

	dprintf(D_SECURITY, "CREDMON: removing %s.", ccfile.c_str());
	unlink(ccfile.c_str());

	//set_priv(priv);
}

bool credmon_poll_for_completion(int cred_type, const char * cred_dir, int timeout)
{
	if (! cred_dir)
		return true;

	const char * type = credmon_type_name(cred_type);

	bool success = false;

	std::string ccfile;
	dircat(cred_dir, "CREDMON_COMPLETE", ccfile);
	for (;;) {
		// look for existence of file that says everything is up-to-date.
		// stat the file as root
		struct stat stat_buf;
		priv_state priv = set_root_priv();
		int rc = stat(ccfile.c_str(), &stat_buf);
		set_priv(priv);
		if (rc == 0) {
			success = true;
			break;
		}
		if (timeout < 0)
			break;
		if (!(timeout % 10)) {
			dprintf(D_ALWAYS, "%s User credentials not up-to-date.  Will wait up to %d more seconds.\n", type, timeout);
		}
		sleep(1);
		--timeout;
	}
	return success;
}

// returns true if there is a credmon and we managed to kick it
bool credmon_kick(int cred_type)
{
	const int read_file_interval = 20;
#if 0 //def WIN32
	const char * file = "wakeid";
	const HANDLE no_handle = INVALID_HANDLE_VALUE;
	static HANDLE krb_handle = no_handle; // handle is an Event handle for Windows
	static HANDLE oauth_handle = no_handle;
	HANDLE * credmon_handle = NULL;
#else
	const char * file = "pid";
	const int no_handle = -1;
	static int krb_handle = no_handle; // handle is the pid for Linux
	static int oauth_handle = no_handle;
	int * credmon_handle = NULL;
#endif
	static time_t krb_credmon_refresh = 0;
	static time_t oauth_credmon_refresh = 0;
	time_t * refresh_time = NULL;

	const char * type = credmon_type_name(cred_type);
	auto_free_ptr cred_dir;

	int now = time(NULL);
	if (cred_type == credmon_type_KRB) {
		credmon_handle = &krb_handle;
		if (krb_handle == no_handle || now > krb_credmon_refresh) {
			cred_dir.set(param("SEC_CREDENTIAL_DIRECTORY_KRB"));
			refresh_time = &krb_credmon_refresh;
		}
	} else if (cred_type == credmon_type_OAUTH) {
		credmon_handle = &oauth_handle;
		if (oauth_handle == no_handle || now > oauth_credmon_refresh) {
			cred_dir.set(param("SEC_CREDENTIAL_DIRECTORY_OAUTH"));
			refresh_time = &oauth_credmon_refresh;
		}
	}
	// no credmon handle, nothing to wake
	if ( ! credmon_handle) {
		return false;
	}

	// cred_dir is set, we need to refresh the credmon handle
	if (cred_dir) {
		std::string idfile;
		dircat(cred_dir, file, idfile);

		int fd = safe_open_no_create(idfile.c_str(), O_RDONLY | _O_BINARY);
		if (fd) {
			char buf[256];
			memset(buf, 0, sizeof(buf));
			size_t len = full_read(fd, buf, sizeof(buf));
			buf[len] = 0;
#if 0 //def WIN32
			HANDLE h = INVALID_HANDLE_VALUE;
			// TODO: Open an event handle
#else
			char * endp = NULL;
			int h = strtol(buf, &endp, 10);
			if (h > 0 && endp > buf) {
				*credmon_handle = h;
			}
#endif
			close(fd);
			*refresh_time = now + read_file_interval;
		}
	}

	// if cred_dir if not set, but we have a credmon handle, we can just kick it.
	if (*credmon_handle != no_handle) {
#ifdef WIN32
		//TODO: signal the credmon
		// SetEvent(*credmon_handle);
		dprintf(D_ALWAYS, "Windows %s credmon: handle=%d (can't send signal)\n", type, *credmon_handle);
#else
		int rc = kill(*credmon_handle, SIGHUP);
		if (rc == -1) {
			dprintf(D_ALWAYS, "failed to signal %s credmon: pid=%d err=%i\n", type, *credmon_handle, errno);
			return false;
		}
#endif
		return true;
	}

	return false;
}


bool credmon_kick_and_poll_for_ccfile(int cred_type, const char * ccfile, int timeout)
{
	const char * type = credmon_type_name(cred_type);

	credmon_kick(cred_type);

	bool success = false;

	for (;;) {
		// look for existence of file that says everything is up-to-date.
		// stat the file as root
		struct stat stat_buf;
		priv_state priv = set_root_priv();
		int rc = stat(ccfile, &stat_buf);
		set_priv(priv);
		if (rc == 0) {
			success = true;
			break;
		}
		if (timeout < 0)
			break;
		if (!(timeout % 10)) {
			dprintf(D_ALWAYS, "%s User credentials not up-to-date.  Will wait up to %d more seconds.\n", type, timeout);
		}
		sleep(1);
		--timeout;
	}
	return success;
}


#if 0

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
		if(param_boolean("CREDD_OAUTH_MODE", false)) {
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
#ifdef WIN32
		//TODO: wake up the credmon
		// SetEvent()
#else
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
#endif
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

#endif

#if 0
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
	int retries = param_integer("CREDD_POLLING_TIMEOUT", 20);
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
#endif

bool credmon_mark_creds_for_sweeping(const char * cred_dir, const char* user,
		int cred_type) {

	if(!cred_dir) {
		return false;
	}

	std::string filename;

	TemporaryPrivSentry sentry(PRIV_ROOT);

	// If there's nothing to clean up, don't create a mark file
	bool need_cleanup = false;
	struct stat stat_buf;
	if (cred_type == credmon_type_OAUTH) {
		credmon_user_filename(filename, cred_dir, user);
		if (stat(filename.c_str(), &stat_buf) == 0) {
			need_cleanup = true;
		}
	} else if (cred_type == credmon_type_KRB) {
		credmon_user_filename(filename, cred_dir, user, ".cred");
		if (stat(filename.c_str(), &stat_buf) == 0) {
			need_cleanup = true;
		}
		credmon_user_filename(filename, cred_dir, user, ".cc");
		if (stat(filename.c_str(), &stat_buf) == 0) {
			need_cleanup = true;
		}
	}

	if (!need_cleanup) {
		return true;
	}

	// construct <cred_dir>/<user>.mark
	credmon_user_filename(filename, cred_dir, user, ".mark");
	FILE* f = safe_fcreate_keep_if_exists(filename.c_str(), "w", 0600);
	if (f == NULL) {
		dprintf(D_ERROR, "CREDMON: ERROR: safe_fcreate_keep_if_exists(%s) failed: %s\n", filename.c_str(), strerror(errno));
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

void process_cred_mark_dir(const char * cred_dir_name, const char *markfile) {

	// in theory, this should never be undefined if we got here, but we're about
	// to recursively delete directories as root so let's bail just to be safe.
	if(!cred_dir_name || !markfile) {
		dprintf(D_ALWAYS, "CREDMON: SWEEPING, but SEC_CREDENTIAL_DIRECTORY_OAUTH not defined!\n");
		return;
	}

	Directory cred_dir(cred_dir_name, PRIV_ROOT);

	dprintf (D_FULLDEBUG, "CREDMON: CRED_DIR: %s, MARK: %s\n", cred_dir_name, markfile );
	if ( cred_dir.Find_Named_Entry( markfile ) ) {
		// it's possible that there are two users named "marky" and
		// "marky.mark".  make sure the marky.mark file is NOT a
		// directory, otherwise we'll delete poor marky's creds.
		if (cred_dir.IsDirectory()) {
			dprintf( D_ALWAYS, "SKIPPING DIRECTORY \"%s\" in %s\n", markfile, cred_dir_name);
			return;
		}

		// also make sure the .mark file is older than the sweep delay.
		time_t sweep_delay = param_integer("SEC_CREDENTIAL_SWEEP_DELAY", 3600);
		time_t now = time(0);
		time_t mtime = cred_dir.GetModifyTime();
		if ( (now - mtime) >= sweep_delay ) {
			dprintf(D_FULLDEBUG, "CREDMON: File %s has mtime %lld which is at least %lld seconds old. Sweeping...\n", markfile, (long long)mtime, (long long)sweep_delay);
		} else {
			dprintf(D_FULLDEBUG, "CREDMON: File %s has mtime %lld which is less than %lld seconds old. Skipping...\n", markfile, (long long)mtime, (long long)sweep_delay);
			return;
		}
	} else {
		dprintf( D_ALWAYS, "CREDMON: Couldn't find dir \"%s\" in %s\n", markfile, cred_dir_name);
		return;
	}

	// delete the mark file (now that we're sure it's not a directory)
	dprintf( D_FULLDEBUG, "Removing %s%c%s\n", cred_dir_name, DIR_DELIM_CHAR, markfile );
	if (!cred_dir.Remove_Current_File()) {
		dprintf( D_ALWAYS, "CREDMON: ERROR REMOVING %s%c%s\n", cred_dir_name, DIR_DELIM_CHAR, markfile );
		return;
	}

	// delete the user's dir
	std::string username = markfile;
	username = username.substr(0, username.length()-5);
	dprintf (D_FULLDEBUG, "CREDMON: CRED_DIR: %s, USERNAME: %s\n", cred_dir_name, username.c_str());
	if ( cred_dir.Find_Named_Entry( username.c_str() ) ) {
		dprintf( D_FULLDEBUG, "Removing %s%c%s\n", cred_dir_name, DIR_DELIM_CHAR, username.c_str() );
		if (!cred_dir.Remove_Current_File()) {
			dprintf( D_ALWAYS, "CREDMON: ERROR REMOVING %s%c%s\n", cred_dir_name, DIR_DELIM_CHAR, username.c_str() );
			return;
		}
	} else {
		dprintf( D_ALWAYS, "CREDMON: Couldn't find dir \"%s\" in %s\n", username.c_str(), cred_dir_name);
		return;
	}
}


void process_cred_mark_file(const char *src) {
	// make sure the .mark file is older than the sweep delay.
	// default is to clean up after 8 hours of no jobs.
	StatInfo si(src);
	if (si.Error()) {
		dprintf(D_ALWAYS, "CREDMON: Error %i trying to stat %s\n", si.Error(), src);
		return;
	}

	int sweep_delay = param_integer("SEC_CREDENTIAL_SWEEP_DELAY", 3600);
	time_t now = time(0);
	time_t mtime = si.GetModifyTime();
	if ( (now - mtime) > sweep_delay ) {
		dprintf(D_FULLDEBUG, "CREDMON: File %s has mtime %lld which is more than %i seconds old. Sweeping...\n", src, (long long)mtime, sweep_delay);
	} else {
		dprintf(D_FULLDEBUG, "CREDMON: File %s has mtime %lld which is more than %i seconds old. Skipping...\n", src, (long long)mtime, sweep_delay);
		return;
	}

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

#if 1

void credmon_sweep_creds(const char * cred_dir, int cred_type)
{
	if ( ! cred_dir || (cred_type != credmon_type_KRB && cred_type != credmon_type_OAUTH))
		return;

#ifdef WIN32
	// TODO: implement this.
#else
	std::string fullpathname;
	dprintf(D_FULLDEBUG, "CREDMON: scandir(%s)\n", cred_dir);
	struct dirent **namelist;
	int n = scandir(cred_dir, &namelist, &markfilter, alphasort);
	if (n >= 0) {
		while (n--) {
			if(cred_type == credmon_type_OAUTH) {
				process_cred_mark_dir(cred_dir, namelist[n]->d_name);
			} else if (cred_type == credmon_type_KRB) {
				dircat(cred_dir, namelist[n]->d_name, fullpathname);
				priv_state priv = set_root_priv();
				process_cred_mark_file(fullpathname.c_str());
				set_priv(priv);
			}
			free(namelist[n]);

		}
		free(namelist);
	} else {
		dprintf(D_FULLDEBUG, "CREDMON: skipping sweep, scandir(%s) got errno %i\n", cred_dir, errno);
	}
#endif
}

#else // old single cred_dir function

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
	std::string fullpathname;
	dprintf(D_FULLDEBUG, "CREDMON: scandir(%s)\n", cred_dir.ptr());
	struct dirent **namelist;
	int n = scandir(cred_dir, &namelist, &markfilter, alphasort);
	if (n >= 0) {
		while (n--) {
			if(param_boolean("CREDD_OAUTH_MODE", false)) {
				process_cred_mark_dir(namelist[n]->d_name);
			} else {
				formatstr(fullpathname, "%s%c%s", cred_dir.ptr(), DIR_DELIM_CHAR, namelist[n]->d_name);
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
#endif


bool credmon_clear_mark(const char * cred_dir , const char* user) {
	if(!cred_dir) {
		return false;
	}

	// construct /<cred_dir>/<user>.mark
	std::string markfile;
	const char * markfilename = credmon_user_filename(markfile, cred_dir, user, ".mark");

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
