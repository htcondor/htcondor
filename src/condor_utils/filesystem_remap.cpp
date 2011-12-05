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
#include "filesystem_remap.h"

#if defined(LINUX)
#include <sys/mount.h>
#endif

int FilesystemRemap::AddMapping(std::string source, std::string dest) {
	if (!is_relative_to_cwd(source) && !is_relative_to_cwd(dest)) {
		std::list<pair_strings>::const_iterator it;
		for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
			if ((it->second.length() == dest.length()) && (it->second.compare(dest) == 0)) {
				dprintf(D_ALWAYS, "Mapping already present for %s.\n", dest.c_str());
				return -1;
			}
		}
		m_mappings.push_back( std::pair<std::string, std::string>(source, dest) );
	} else {
		dprintf(D_ALWAYS, "Unable to add mappings for relative directories (%s, %s).\n", source.c_str(), dest.c_str());
		return -1;
	}
	return 0;
}

// This is called within the exec
// IT CANNOT CALL DPRINTF!
int FilesystemRemap::PerformMappings() {
	int retval = 0;
#if defined(LINUX)
	std::list<pair_strings>::iterator it;
	for (it = m_mappings.begin(); it != m_mappings.end(); it++) {
		if ((retval = mount(it->first.c_str(), it->second.c_str(), NULL, MS_BIND, NULL))) {
			break;
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
