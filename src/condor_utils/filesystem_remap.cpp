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
#include "condor_uid.h"
#include "filesystem_remap.h"
#include "condor_config.h"
#include "directory.h"
#include "my_popen.h"
#include "condor_daemon_core.h"
#include "basename.h"
#include "../condor_sysapi/sysapi.h"

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

FilesystemRemap::FilesystemRemap() :
	m_mappings(),
	m_mounts_shared(),
	m_remap_proc(false)
{
	ParseMountinfo();
	FixAutofsMounts();
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

	// do bind mounts (or chroots) for AddMapping() entries
	for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
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

#define ADVANCE_TOKEN(token, sti, full_str) {	\
	if ((token = sti.next()) == NULL) { \
		fclose(fd); \
		dprintf(D_ALWAYS, "Invalid line in mountinfo file: %s\n", full_str.c_str()); \
		return; \
	} \
}

#define SHARED_STR "shared:"

void FilesystemRemap::ParseMountinfo() {

#if defined(LINUX)

	std::string str2;
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

	while (readLine(str2, fd, false)) {
		StringTokenIterator sti(str2);
		ADVANCE_TOKEN(token, sti, str2); // mount ID
		ADVANCE_TOKEN(token, sti, str2); // parent ID
		ADVANCE_TOKEN(token, sti, str2); // major:minor
		ADVANCE_TOKEN(token, sti, str2); // root
		ADVANCE_TOKEN(token, sti, str2); // mount point
		std::string mp(token);
		ADVANCE_TOKEN(token, sti, str2); // mount options
		ADVANCE_TOKEN(token, sti, str2); // optional fields
		is_shared = false;
		while (strcmp(token, "-") != 0) {
			is_shared = is_shared || (strncmp(token, SHARED_STR, strlen(SHARED_STR)) == 0);
			ADVANCE_TOKEN(token, sti, str2);
		}
		ADVANCE_TOKEN(token, sti, str2); // filesystem type
		if ((!is_shared) && (strcmp(token, "autofs") == 0)) {
			ADVANCE_TOKEN(token, sti, str2);
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
		for (const auto& next_chroot: StringTokenIterator(allowed_root_dirs)) {
			StringTokenIterator chroot_spec(next_chroot, "=");
			const char* tok;
			tok = chroot_spec.next();
			if (tok == NULL) {
				dprintf(D_ALWAYS, "Invalid named chroot: %s\n", next_chroot.c_str());
				continue;
			}
			std::string chroot_name = tok;
			tok = chroot_spec.next();
			if (tok == NULL) {
				dprintf(D_ALWAYS, "Invalid named chroot: %s\n", next_chroot.c_str());
				continue;
			}
			std::string next_dir = tok;
			if (IsDirectory(next_dir.c_str())) {
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

