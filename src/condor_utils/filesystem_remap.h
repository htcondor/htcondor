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

#ifndef FILESYSTEM_REMAP_H
#define FILESYSTEM_REMAP_H

#include <string>
#include <list>
#include <vector>
#include <utility>



/**
 * Represents a set of mappings to perform on the filesystem.
 *
 * Contains a mapping of source directories that will be remounted to destinations.
 * So, if we have the mapping (/var/lib/condor/execute/1234) -> (/tmp), this class
 * will perform the equivalent of this command:
 *    mount --bind /var/lib/condor/execute/1234 /tmp
 * This is meant to give Condor the ability to provide per-job temporary directories.
 *
 * The class does two special things:
 *   1) For each remapping, if the parent mount is marked as shared-subtree, remount
 *      it first as non-shared-subtree.  This prevents remappings we make from
 *      escaping to the parent.
 *   2) Promote each autofs mount to shared-subtree.  Autofs is not aware of
 *      namespaces and when a job accesses the mount point in the job's namespaces,
 *      autofs will do the real mount in the *parent* namespace, so the job cannot
 *      access it unless we do the shared-subtree trick.
 * 
 */
typedef std::pair<std::string, std::string> pair_strings;
typedef std::pair<std::string, bool> pair_str_bool;
typedef std::vector<pair_strings> pair_strings_vector;

class FilesystemRemap {

public:

	FilesystemRemap();

	/**
	 * Add a mapping to the filesystem remap.
	 * @param source: A source directory that will be remapped to the destination.
	 * @param dest: A destination directory
	 * @returns: 0 on success, -1 if the directories were not mappable.
	 */
	int AddMapping(std::string source, std::string dest);

	/**
	 * Add a ecryptfs mountpoint mapping.
	 * @param mountpoint: Both the source and dest for ecryptfs mount.
	 * @param password: Password for encryption; if NULL, pick at random.
	 * @returns: 0 on success, -1 if the directories were not mappable.
	 */
	int AddEncryptedMapping(std::string mountpoint, std::string password = "");

	/**
	 * Make /dev/shm a private mount
     * this gives each job their own view, and cleans up /dev/shm on exit
     */
	int AddDevShmMapping();
	/**
	 * Performs the mappings known to this class.
	 * This method does not touch the privilege settings - the caller is responsible
	 * for setting the appropriate context.  This is done because the primary usage
	 * of this class is the 'exec' part of the clone, where we have special rules
	 * for touching shared memory.
	 * @returns: 0 if everything went well, -1 if the remounts failed.
	 */
	int PerformMappings();

	/**
	 * Determine where a directory will be accessible from after the mapping.
	 * @param Directory to consider.
	 * @return Renamed directory.
	 */
	std::string RemapDir(const std::string);

	/**
	 * Determine where a file will be accessible from after the mapping.
	 * @param Directory to consider.
	 * @return Renamed directory.
	 */
	std::string RemapFile(std::string);

	/**
	 * Indicate that we should remount /proc in the child process.
	 * Necessary for PID namespaces.
	 */
	void RemapProc();

	/**
	 * Remove any keys stashed in the kernel for ecryptfs.
	 * Normally should only be called from daemonCore DC_Exit(), i.e. only
	 * invoke when process is exiting.
	 */
	static void EcryptfsUnlinkKeys();

	/**
	 * Can this system perform encrypted mappings?
	 * Returns true if it can, false if not
	 */
	static bool EncryptedMappingDetect();

private:

	/**
	 * Parse /proc/self/mountinfo file; look for mounts that are shared
	 */
	void ParseMountinfo();

	/**
	 * Check to see if the desired mount point is going to be shared
	 * outside the current namespace.  If so, remount it as private.
	 */
	int CheckMapping(const std::string &);

	/**
	 * autofs does not understand namespaces and ends up making the "real" fs
	 * invisible to the job - it goes into the parent namespace instead.  We
	 * provide this hack to remount autofs mounts as shared-subtree so changes
	 * to the parent namespace are visible to all jobs.
	 */
	int FixAutofsMounts();

	std::list<pair_strings> m_mappings;
	std::list<pair_str_bool> m_mounts_shared;
	std::list<pair_strings> m_mounts_autofs;

	bool m_remap_proc;

	std::list<pair_strings> m_ecryptfs_mappings;
	static std::string m_sig1;
	static std::string m_sig2;
	static int m_ecryptfs_tid;
	static bool EcryptfsGetKeys(int & key1, int & key2);
	static void EcryptfsRefreshKeyExpiration();
};

/**
 * Get a list of the named chroots
 */
pair_strings_vector root_dir_list();

/**
 * Given a chroot directory, make sure it isn't equivalent to "/"
 */
bool is_trivial_rootdir(const std::string &root_dir);

#endif
