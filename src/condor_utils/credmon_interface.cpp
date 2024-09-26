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

	dprintf(D_SECURITY, "CREDMON: removing %s.\n", ccfile.c_str());
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

	time_t now = time(nullptr);
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


bool credmon_mark_creds_for_sweeping(const char * cred_dir, const char* user,
		int cred_type) {

	if(!cred_dir) {
		return false;
	}

	std::string filename;
	const char* cred_type_name = "";

	TemporaryPrivSentry sentry(PRIV_ROOT);

	// If there's nothing to clean up, don't create a mark file
	bool need_cleanup = false;
	struct stat stat_buf;
	if (cred_type == credmon_type_OAUTH) {
		cred_type_name = "OAUTH";
		credmon_user_filename(filename, cred_dir, user);
		if (stat(filename.c_str(), &stat_buf) == 0) {
			need_cleanup = true;
		}
	} else if (cred_type == credmon_type_KRB) {
		cred_type_name = "KRB";
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

	dprintf(D_FULLDEBUG, "CREDMON: Creating %s mark file for user %s\n", cred_type_name, user);

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
