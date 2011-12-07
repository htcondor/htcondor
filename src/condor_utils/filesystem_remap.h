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
#include <utility>

/**
 * Represents a set of mappings to perform on the filesystem.
 *
 * Contains a mapping of source directories that will be remounted to destinations.
 * So, if we have the mapping (/var/lib/condor/execute/1234) -> (/tmp), this class
 * will perform the equivalent of this command:
 *    mount --bind /var/lib/condor/execute/1234 /tmp
 * This is meant to give Condor the ability to provide per-job temporary directories.
 */
typedef std::pair<std::string, std::string> pair_strings;
typedef std::pair<std::string, bool> pair_str_bool;

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

	std::list<pair_strings> m_mappings;
	std::list<pair_str_bool> m_mounts_shared;

};
#endif
