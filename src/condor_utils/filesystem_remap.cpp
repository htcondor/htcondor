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

#include "filename_tools.h"
#include "condor_debug.h"
#include "MyString.h"
#include "condor_uid.h"
#include "filesystem_remap.h"
#include "condor_config.h"
#include "directory.h"
#include "my_popen.h"
#include "condor_daemon_core.h"
#include "basename.h"

#if defined(LINUX)
#include <sys/utsname.h>
#include <sys/mount.h>
// Define some syscall keyring constants.  Normally these are in
// <keyutils.h>, but we only need a few values that are not changing,
// and by placing them here we do not require keyutils development
// package from being installed
#ifndef KEYCTL_JOIN_SESSION_KEYRING
  #define KEYCTL_JOIN_SESSION_KEYRING 1
  #define KEYCTL_SEARCH 10
  #define KEYCTL_SET_TIMEOUT 15
  #define KEYCTL_UNLINK 9
  #define KEY_SPEC_USER_KEYRING -4
#endif  // of ifndef KEYCTL_JOIN_SESSION_KEYRING
#endif  // of defined(LINUX)

// Initialize static data members
std::string FilesystemRemap::m_sig1 = "";
std::string FilesystemRemap::m_sig2 = "";
int FilesystemRemap::m_ecryptfs_tid = -1;

FilesystemRemap::FilesystemRemap() :
	m_mappings(),
	m_mounts_shared(),
	m_remap_proc(false)
{
	ParseMountinfo();
	FixAutofsMounts();
}

bool FilesystemRemap::EcryptfsGetKeys(int & key1, int & key2)
{
	bool retval = false;

#ifdef LINUX
	key1 = -1;
	key2 = -1;

	if (m_sig1.length() > 0 && m_sig2.length() > 0) {
		TemporaryPrivSentry sentry(PRIV_ROOT);  // root privs to search root keyring
		key1 = syscall(__NR_keyctl, KEYCTL_SEARCH, KEY_SPEC_USER_KEYRING, "user",
				m_sig1.c_str(), 0);
		key2 = syscall(__NR_keyctl, KEYCTL_SEARCH, KEY_SPEC_USER_KEYRING, "user",
				m_sig2.c_str(), 0);
		if (key1 == -1 || key2 == -1) {
			dprintf(D_ALWAYS,"Failed to fetch serial num for encryption keys (%s,%s)\n",
					m_sig1.c_str(),m_sig2.c_str());
			m_sig1 = "";
			m_sig2 = "";
			key1 = -1;
			key2 = -1;
		} else {
			retval = true;
		}
	}
#else
	(void) key1;
	(void) key2;
#endif

	return retval;
}

void FilesystemRemap::EcryptfsRefreshKeyExpiration()
{
#ifdef LINUX
	int key1,key2;

	if (EcryptfsGetKeys(key1,key2)) {
		int key_timeout = param_integer("ECRYPTFS_KEY_TIMEOUT");
		TemporaryPrivSentry sentry(PRIV_ROOT);  // root privs to search root keyring
		syscall(__NR_keyctl, KEYCTL_SET_TIMEOUT, key1, key_timeout);
		syscall(__NR_keyctl, KEYCTL_SET_TIMEOUT, key2, key_timeout);
	} else {
		// If the keys disappeared and jobs are trying to run in an encrypted
		// directory, the jobs will get all sorts of I/O failures.
		// EXCEPT here to let an admin know something is really wrong.
		// This should never happen...
		EXCEPT("Encryption keys disappeared from kernel - jobs unable to write");
	}
#endif

	return;
}

void FilesystemRemap::EcryptfsUnlinkKeys()
{
#ifdef LINUX
	if (m_ecryptfs_tid != -1) {
		daemonCore->Cancel_Timer( m_ecryptfs_tid );
		m_ecryptfs_tid = -1;
	}
	int key1,key2;
	if (EcryptfsGetKeys(key1,key2)) {
		TemporaryPrivSentry sentry(PRIV_ROOT);  // root privs to search root keyring
		syscall(__NR_keyctl, KEYCTL_UNLINK, key1, KEY_SPEC_USER_KEYRING);
		syscall(__NR_keyctl, KEYCTL_UNLINK, key2, KEY_SPEC_USER_KEYRING);
		m_sig1 = "";
		m_sig2 = "";
	}
#endif

	return;
}

bool FilesystemRemap::EncryptedMappingDetect()
{
#ifdef LINUX
	static int answer = -1;  // cache the answer we discover

	// if we already know the answer cuz we figured it out in the
	// past, return now
	if ( answer != -1 ) {
		return answer ? true : false;
	}

	// Here we need to figure out if we can do encrypted exec dirs.
	// We need to confirm:
	// 1. we are running with root access
	// 2. PER_JOB_NAMESPACES knob is set to true (default)
	// 3. ecryptfs-add-passphrase tool is installed
	// 4. ecryptfs module is available in this kernel
	// 5. kernel version is 2.6.29 or above (because earlier versions
	//    of the kernel did not support file name encryption properly)
	// 6. that we can discard/change session keyrings

	// 1. we are running with root access
	if (!can_switch_ids()) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: not running as root\n");
		answer = FALSE;
		return false;
	}

	// 2. PER_JOB_NAMESPACES knob is set to true (default)
	if (param_boolean("PER_JOB_NAMESPACES",true)==false) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: PER_JOB_NAMESPACES is false\n");
		answer = FALSE;
		return false;
	}

	// 3. ecryptfs-add-passphrase tool is installed
	char *addpasspath = param_with_full_path("ECRYPTFS_ADD_PASSPHRASE");
	if (addpasspath == NULL) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: failed to find ecryptfs-add-passphrase\n");
		answer = FALSE;
		return false;
	}
	free(addpasspath);

	// 4. ecryptfs module is available in this kernel
	// SKIPPING THIS STEP FOR NOW.... some distros (Ubuntu) have
	// ecryptfs built directly into the kernel, so will not appear
	// as a loadable module.  Plus practially all
	// recentl kernels/distros support ecryptfs, esp if
	// the ecryptfs utils are available on the system  :).
	// Thus commenting out the below...
#if 0
	char *modprobe = param_with_full_path("modprobe");
	if (!modprobe) {
		// cannot find modprobe in path, try /sbin
		modprobe = strdup("/sbin/modprobe");
	}
	ArgList cmdargs;
	cmdargs.AppendArg(modprobe);
	free(modprobe); modprobe=NULL;
	cmdargs.AppendArg("-l");
	cmdargs.AppendArg("ecryptfs");
	FILE * stream = my_popen(cmdargs, "r", FALSE);
	if (!stream) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: Failed to run %s\n, ",cmdargs.GetArg(0));
		answer = FALSE;
		return false;
	}
	char buf[80];
	buf[0] = '\0';
	fgets(buf,sizeof(buf),stream);
	my_pclose(stream);
	if (!strstr(buf,"ecryptfs")) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: kernel module ecryptfs not found\n");
		answer = FALSE;
		return false;
	}
#endif

	// 5. kernel version is 2.6.29 or above - I've read about
	// bugs in --fnek support in kernels older than that.
	if ( !sysapi_is_linux_version_atleast("2.6.29") ) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: kernel version older than 2.6.29\n");
		answer = FALSE;
		return false;
	}

	// 6. that we can discard/change session keyrings
	if (param_boolean("DISCARD_SESSION_KEYRING_ON_STARTUP",true)==false) {
		dprintf(D_FULLDEBUG,
				"EncryptedMappingDetect: DISCARD_SESSION_KEYRING_ON_STARTUP=false\n");
		answer = FALSE;
		return false;
	} else {
		if (syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, "htcondor")==-1) {
			dprintf(D_FULLDEBUG,
					"EncryptedMappingDetect: failed to discard session keyring\n");
			answer = FALSE;
			return false;
		}
	}

	// if made it here, looks like we can do it
	answer = TRUE;
	return true;
#else
	// For any non-Linux platform, return false
	return false;
#endif
}


int FilesystemRemap::AddMapping(std::string source, std::string dest) {
	if (fullpath(source.c_str()) && fullpath(dest.c_str())) {
		std::list<pair_strings>::const_iterator it;
		for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
			if ((it->second.length() == dest.length()) && (it->second.compare(dest) == 0)) {
				// return success (i.e. not a fatal error to repeat a mapping)
				return 0;
			}
		}
		if (CheckMapping(dest)) {
			dprintf(D_ALWAYS, "Failed to convert shared mount to private mapping");
			return -1;
		}
		m_mappings.push_back( std::pair<std::string, std::string>(source, dest) );
	} else {
		dprintf(D_ALWAYS, "Unable to add mappings for relative directories (%s, %s).\n", source.c_str(), dest.c_str());
		return -1;
	}
	return 0;
}

int FilesystemRemap::AddEncryptedMapping(std::string mountpoint, std::string password)
{
#if defined(LINUX)
	if (!EncryptedMappingDetect()) {
		dprintf(D_ALWAYS, "Unable to add encrypted mappings: not supported on this machine\n");
		return -1;
	}
	if (!fullpath(mountpoint.c_str())) {
		dprintf(D_ALWAYS, "Unable to add encrypted mappings for relative directories (%s).\n",
				mountpoint.c_str());
		return -1;
	}
	std::list<pair_strings>::const_iterator it;
	for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
		if ((it->first.length() == mountpoint.length()) &&
			(it->first.compare(mountpoint) == 0))
		{
			// return success (i.e. not a fatal error to repeat a mapping)
			return 0;
		}
	}

	// Make this mount point private so cleartext is not visible outside of the
	// process tree
	if (CheckMapping(mountpoint)) {
		dprintf(D_ALWAYS, "Failed to convert shared mount to private mapping (%s)\n",
				mountpoint.c_str());
		return -1;
	}

		// If no password given, create a random one
	if (password.length() == 0) {
		randomlyGenerateShortLivedPassword(password, 28);  // 28 chars long
	}

	// Put password into user root keyring stored in the kernel via
	// the ecryptfs-add-passphrase command-line tool.
	ArgList cmdargs;
	int ret;
	int key1 = -1;
	int key2 = -1;
	char *addpasspath = param_with_full_path("ECRYPTFS_ADD_PASSPHRASE");
	if (!addpasspath) {
		dprintf(D_ALWAYS, "Failed to locate encryptfs-add-pasphrase\n");
		return -1;
	}
	cmdargs.AppendArg(addpasspath);
	free(addpasspath); addpasspath=NULL;
	cmdargs.AppendArg("--fnek");
	cmdargs.AppendArg("-");
	if (!EcryptfsGetKeys(key1,key2)) {  // if cannot get keys, must create them
		// must do popen as root so root keyring is used
		TemporaryPrivSentry sentry(PRIV_ROOT);
		FILE * stream = my_popen(cmdargs, "r", FALSE, NULL, false, password.c_str());
		if (!stream) {
			dprintf(D_ALWAYS,"Failed to run %s\n, ",cmdargs.GetArg(0));
			return -1;
		}
		char sig1[80],sig2[80];  // sig hashes should only be ~16 chars, so 80 is futureproof
		sig1[0] = '\0'; sig2[0] = '\0';
		// output is "blah blah ... [sig1] blah blah [sig2] ...", and
		// we want to grab the sig1 and sig2 strings contained with the
		// square brackets.  so just use fscanf with ToddT's mad scanf skilzzz.
		int sfret = fscanf(stream,"%*[^[][%79[^]]%*[^[][%79[^]]",sig1,sig2);
		ret = my_pclose(stream); // ret now has exit status from ecryptfs-add-passphrase
		if (ret != 0 || sfret != 2 || !sig1[0] || !sig2[0]) {
			dprintf(D_ALWAYS,
					"%s failed to store encyption and file name encryption keys (%d,%s,%s)\n",
					cmdargs.GetArg(0),ret,sig1,sig2);
			return -1;
		}
		// Stash the signatures into our static member variables
		m_sig1 = sig1;
		m_sig2 = sig2;
		// Set an expiration timeout of 60 minutes (by default) on the keys
		EcryptfsRefreshKeyExpiration();
	} // end of PRIV_ROOT here (sentry out of scope)

	// Register periodic timer to refresh expiration time on the keys every 5 min
	if (m_ecryptfs_tid == -1) {
		m_ecryptfs_tid = daemonCore->Register_Timer(300,300,
				(TimerHandler)&FilesystemRemap::EcryptfsRefreshKeyExpiration,
				"EcryptfsRefreshKeyExpiration");
		ASSERT( m_ecryptfs_tid >= 0);
	}

	// create mount options line
	std::string mountopts;
	formatstr(mountopts,
			"ecryptfs_sig=%s,ecryptfs_cipher=aes,ecryptfs_key_bytes=16",
			m_sig1.c_str());

	// optionally encrypt the filenames themselves
	if(param_boolean("ENCRYPT_EXECUTE_DIRECTORY_FILENAMES",false)) {
		mountopts += ",ecryptfs_fnek_sig=" + m_sig2;
	}

	// stash mount info for PerformMappings() to access.  we do this in advance as
	// we don't want PerformMappings doing heap memory allocation/destructions.
	m_ecryptfs_mappings.push_back( std::pair<std::string, std::string>(mountpoint, mountopts) );
#else
	(void) mountpoint;
	(void) password;
#endif  // of if defined(LINUX)

	return 0;
}

int 
FilesystemRemap::AddDevShmMapping() {
#ifdef LINUX
	if (!param_boolean("MOUNT_PRIVATE_DEV_SHM", true)) {
		return 1;
	}
    TemporaryPrivSentry sentry(PRIV_ROOT);

	// Re-mount the mount point as a bind mount, so we can subsequently
	// re-mount it as private.
	
	if (mount("/dev/shm", "/dev/shm", "tmpfs", 0, NULL)) {	
		dprintf(D_ALWAYS, "Marking /dev/shm as a bind mount failed. (errno=%d, %s)\n", errno, strerror(errno));
		return -1;
	}

#ifdef HAVE_MS_PRIVATE
	if (mount("none", "/dev/shm", NULL, MS_PRIVATE, NULL)) {
		dprintf(D_ALWAYS, "Marking /dev/shm as a private mount failed. (errno=%d, %s)\n", errno, strerror(errno));
		return -1;
	} else {
		dprintf(D_FULLDEBUG, "Mounting /dev/shm as a private mount successful.\n");
	}
#endif

	return 0;
#else
	return 0;
#endif
}

int FilesystemRemap::CheckMapping(const std::string & mount_point) {
#ifndef HAVE_UNSHARE
	dprintf(D_ALWAYS, "This system doesn't support remounting of filesystems: %s\n", mount_point.c_str());
	return -1;
#else
	bool best_is_shared = false;
	size_t best_len = 0;
	const std::string *best = NULL;

	dprintf(D_FULLDEBUG, "Checking the mapping of mount point %s.\n", mount_point.c_str());

	for (std::list<pair_str_bool>::const_iterator it = m_mounts_shared.begin(); it != m_mounts_shared.end(); it++) {
		std::string first = it->first;
		if ((strncmp(first.c_str(), mount_point.c_str(), first.size()) == 0) && (first.size() > best_len)) {
			best_len = first.size();
			best = &(it->first);
			best_is_shared = it->second;
		}
	}

	if (!best_is_shared) {
		return 0;
	}
    
    dprintf(D_ALWAYS, "Current mount, %s, is shared.\n", best->c_str());
    
#if !defined(HAVE_MS_SLAVE) && !defined(HAVE_MS_REC)
    TemporaryPrivSentry sentry(PRIV_ROOT);

	// Re-mount the mount point as a bind mount, so we can subsequently
	// re-mount it as private.
	
	if (mount(mount_point.c_str(), mount_point.c_str(), NULL, MS_BIND, NULL)) {	
		dprintf(D_ALWAYS, "Marking %s as a bind mount failed. (errno=%d, %s)\n", mount_point.c_str(), errno, strerror(errno));
		return -1;
	}

#ifdef HAVE_MS_PRIVATE
	if (mount(mount_point.c_str(), mount_point.c_str(), NULL, MS_PRIVATE, NULL)) {
		dprintf(D_ALWAYS, "Marking %s as a private mount failed. (errno=%d, %s)\n", mount_point.c_str(), errno, strerror(errno));
		return -1;
	} else {
		dprintf(D_FULLDEBUG, "Marking %s as a private mount successful.\n", mount_point.c_str());
	}
#endif

#endif

	return 0;
#endif
}

int FilesystemRemap::FixAutofsMounts() {
#ifndef HAVE_UNSHARE
	// An appropriate error message is printed in FilesystemRemap::CheckMapping;
	// Not doing anything here.
	return -1;
#else

#ifdef HAVE_MS_SHARED
	TemporaryPrivSentry sentry(PRIV_ROOT);
	for (std::list<pair_strings>::const_iterator it=m_mounts_autofs.begin(); it != m_mounts_autofs.end(); it++) {
		if (mount(it->first.c_str(), it->second.c_str(), NULL, MS_SHARED, NULL)) {
			dprintf(D_ALWAYS, "Marking %s->%s as a shared-subtree autofs mount failed. (errno=%d, %s)\n", it->first.c_str(), it->second.c_str(), errno, strerror(errno));
			return -1;
		} else {
			dprintf(D_FULLDEBUG, "Marking %s as a shared-subtree autofs mount successful.\n", it->second.c_str());
		}
	}
#endif
	return 0;
#endif
}

// This is called  by daemoncore between the fork() and exec(), with root powers if available
// IT CANNOT CALL DPRINTF! (... actually, ToddT thinks that it can nowadays 2/2105)
int FilesystemRemap::PerformMappings() {
	int retval = 0;
#if defined(LINUX)
	std::list<pair_strings>::iterator it;

	// If we have AddEncryptedMapping entries, we must switch to the root
	// session's keyring before doing ecryptfs mounts below.  This is because
	// the ecryptfs-add-passphrase utility (spawned when the mapping was added) inserted
	// the mount key into the root user keyring, and the mount syscall only searches the
	// current session keyring.
	// We switch session keyrings via direct syscall so we don't have to rely on
	// pulling in yet another shared library (and thus worrying about if the library
	// keyutils.so is on the system, increasing memory footprint of the shadow,
	// the existance of the header file on the build machine, etc).
	// -Todd Tannenbaum 1/2015.
	if (m_ecryptfs_mappings.size() > 0) {
		// (no need to check for err codes; if this fails, the mount below
		// will fail)
		syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, "_uid.0");
		// Note: DaemonCore should take care of creating a new empty session keyring
		// (without a link to the ecryptfs keys) when spawing a PRIV_USER_FINAL
		// process, thereby ensuring we don't leak the keys to a user job.
	}

	// do mounts for AddEncryptedMapping() entries
	for (it = m_ecryptfs_mappings.begin(); it != m_ecryptfs_mappings.end(); it++) {
		if ((retval = mount(it->first.c_str(), it->first.c_str(),
						"ecryptfs", 0, it->second.c_str())))
		{
			dprintf(D_ALWAYS,"Filesystem Remap failed mount -t ecryptfs %s %s: %s (errno=%d)\n",
				it->first.c_str(), it->second.c_str(), strerror(errno), errno);
			break;
		}
	}

	// If we switched to the root session keyring above, undo that
	// here so the user job cannot see keys on root keyring.
	if (m_ecryptfs_mappings.size() > 0) {
		if (syscall(__NR_keyctl, KEYCTL_JOIN_SESSION_KEYRING, "htcondor")==-1) {
			retval = 1;
			dprintf(D_ALWAYS,"Filesystem Remap new session keying failed: %s (errno=%d)\n",
				strerror(errno), errno);
		} else {
			retval = 0; // below code assumes retval==0 on failures
		}
	}

	// do bind mounts (or chroots) for AddMapping() entries
	if (!retval) for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
		if (strcmp(it->second.c_str(), "/") == 0) {
			if ((retval = chroot(it->first.c_str()))) {
				break;
			}
			if ((retval = chdir("/"))) {
				break;
			}
		} else if ((retval = mount(it->first.c_str(), it->second.c_str(), NULL, MS_BIND, NULL))) {
			break;
		}
	}

	if (!retval) {
		AddDevShmMapping();
	}

	// do mounts for RemapProc()
	if ((!retval) && m_remap_proc) {
    	TemporaryPrivSentry sentry(PRIV_ROOT);
		retval = mount("proc", "/proc", "proc", 0, NULL);
		if (retval < 0) {
			dprintf(D_ALWAYS, "Cannot remount proc, errno is %d\n", errno);
		}
	}
#endif
	return retval;
}

std::string FilesystemRemap::RemapFile(std::string target) {
	if (target[0] != '/')
		return std::string();
	size_t pos = target.rfind("/");
	if (pos == std::string::npos)
		return target;
	std::string filename = target.substr(pos, target.size() - pos);
	std::string directory = target.substr(0, target.size() - filename.size());
	return RemapDir(directory) + filename;
}

std::string FilesystemRemap::RemapDir(std::string target) {
	if (target[0] != '/')
		return std::string();
	std::list<pair_strings>::iterator it;
	for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
		if ((it->first.compare(0, it->first.length(), target, 0, it->first.length()) == 0)
				&& (it->second.compare(0, it->second.length(), it->first, 0, it->second.length()) == 0)) {
			target.replace(0, it->first.length(), it->second);
		}
	}
	return target;
}

void FilesystemRemap::RemapProc() {
	m_remap_proc = true;
}

/*
  Sample mountinfo contents (from http://www.kernel.org/doc/Documentation/filesystems/proc.txt):
  36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
  (1)(2)(3)   (4)   (5)      (6)      (7)   (8) (9)   (10)         (11)

  (1) mount ID:  unique identifier of the mount (may be reused after umount)
  (2) parent ID:  ID of parent (or of self for the top of the mount tree)
  (3) major:minor:  value of st_dev for files on filesystem
  (4) root:  root of the mount within the filesystem
  (5) mount point:  mount point relative to the process's root
  (6) mount options:  per mount options
  (7) optional fields:  zero or more fields of the form "tag[:value]"
  (8) separator:  marks the end of the optional fields
  (9) filesystem type:  name of filesystem of the form "type[.subtype]"
  (10) mount source:  filesystem specific information or "none"
  (11) super options:  per super block options
 */

#define ADVANCE_TOKEN(token, str) { \
	if ((token = str.GetNextToken(" ", false)) == NULL) { \
		fclose(fd); \
		dprintf(D_ALWAYS, "Invalid line in mountinfo file: %s\n", str.Value()); \
		return; \
	} \
}

#define SHARED_STR "shared:"

void FilesystemRemap::ParseMountinfo() {

#if defined(LINUX)

	MyString str2;
	const char * token;
	FILE *fd;
	bool is_shared;

	if ((fd = fopen("/proc/self/mountinfo", "r")) == NULL) {
		if (errno == ENOENT) {
			dprintf(D_FULLDEBUG, "The /proc/self/mountinfo file does not exist; kernel support probably lacking.  Will assume normal mount structure.\n");
		} else {
			dprintf(D_ALWAYS, "Unable to open the mountinfo file (/proc/self/mountinfo). (errno=%d, %s)\n", errno, strerror(errno));
		}
		return;
	}

	while (str2.readLine(fd, false)) {
		MyStringWithTokener str(str2);
		str.Tokenize();
		ADVANCE_TOKEN(token, str) // mount ID
		ADVANCE_TOKEN(token, str) // parent ID
		ADVANCE_TOKEN(token, str) // major:minor
		ADVANCE_TOKEN(token, str) // root
		ADVANCE_TOKEN(token, str) // mount point
		std::string mp(token);
		ADVANCE_TOKEN(token, str) // mount options
		ADVANCE_TOKEN(token, str) // optional fields
		is_shared = false;
		while (strcmp(token, "-") != 0) {
			is_shared = is_shared || (strncmp(token, SHARED_STR, strlen(SHARED_STR)) == 0);
			ADVANCE_TOKEN(token, str)
		}
		ADVANCE_TOKEN(token, str) // filesystem type
		if ((!is_shared) && (strcmp(token, "autofs") == 0)) {
			ADVANCE_TOKEN(token, str)
			m_mounts_autofs.push_back(pair_strings(token, mp));
		}
		// This seems a bit too chatty - disabling for now.
		// dprintf(D_FULLDEBUG, "Mount: %s, shared: %d.\n", mp.c_str(), is_shared);
		m_mounts_shared.push_back(pair_str_bool(mp, is_shared));
	}

	fclose(fd);
#endif  // of defined(LINUX)
}

pair_strings_vector
root_dir_list()
{
	pair_strings_vector execute_dir_list;
	execute_dir_list.push_back(pair_strings("root","/"));
	const char * allowed_root_dirs = param("NAMED_CHROOT");
	if (allowed_root_dirs) {
		StringList chroot_list(allowed_root_dirs);
		chroot_list.rewind();
		const char * next_chroot;
		while ( (next_chroot=chroot_list.next()) ) {
			MyStringWithTokener chroot_spec(next_chroot);
			chroot_spec.Tokenize();
			const char * chroot_name = chroot_spec.GetNextToken("=", false);
			if (chroot_name == NULL) {
				dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.c_str());
				continue;
			}
			const char * next_dir = chroot_spec.GetNextToken("=", false);
			if (next_dir == NULL) {
				dprintf(D_ALWAYS, "Invalid named chroot: %s\n", chroot_spec.c_str());
				continue;
			}
			if (IsDirectory(next_dir)) {
				pair_strings p(chroot_name, next_dir);
				execute_dir_list.push_back(p);
			}
		}
	}
	return execute_dir_list;
}

bool
is_trivial_rootdir(const std::string &root_dir)
{
	for (std::string::const_iterator it=root_dir.begin(); it!=root_dir.end(); it++) {
		if (*it != '/')
			return false;
	}
	return true;
}

