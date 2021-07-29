/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#include "ickpt_share.h"

#include <iomanip>
#include <sstream>

#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "link.h"

extern char* Spool;

// escape a string to that it is acceptable for use as a filename. any
// character not in the regex [a-zA-Z0-9._] is replaced with %AA, where
// AA is the character's two-hex-digit equivalent
//
static std::string
escape_for_filename(const std::string& s)
{
	std::ostringstream out;
	out << std::hex << std::setfill('0');
	for (std::string::const_iterator i = s.begin(); i != s.end(); i++) {
		if (isalnum(*i) || (*i == '_') || (*i == '.')) {
			out << *i;
		}
		else {
			out << '%' << std::setw(2) << static_cast<int>(*i);
		}
	}
	return out.str();
}

// given an owner and an executable's hash string, construct the filename
// used to hold the shared ickpt file. note that the hash argument is
// expected to already be escaped for use in a filename
//
static std::string
make_hash_filename(const std::string& owner, const std::string& hash)
{
	return std::string(Spool) +
	       "/exe-" +
	       escape_for_filename(owner) +
	       "-" +
	       hash;
}

std::string
ickpt_share_get_hash(ClassAd& ad)
{
	// for now, we only pay attention to the executable's checksum
	//
	std::string hash;
	if (!ad.LookupString(ATTR_JOB_CMD_CHECKSUM, hash)) {
		return "";
	}
	return escape_for_filename(ATTR_JOB_CMD_CHECKSUM) +
	       "-" +
	       escape_for_filename(hash.c_str());
}

bool
ickpt_share_try_sharing(const std::string& owner,
                        const std::string& hash,
                        const std::string& ickpt_file)
{
	std::string hash_file = make_hash_filename(owner, hash);
	if (link(hash_file.c_str(), ickpt_file.c_str()) == -1) {
		if (errno == ENOENT) {
			dprintf(D_FULLDEBUG,
			        "ickpt_share: %s not available for link from %s\n",
			        hash_file.c_str(),
			        ickpt_file.c_str());
		}
		else {
			dprintf(D_ALWAYS,
			        "ickpt_share: unexpected error linking %s to %s: %s\n",
			        ickpt_file.c_str(),
			        hash_file.c_str(),
			        strerror(errno));
		}
		return false;
	}
	dprintf(D_FULLDEBUG,
	        "ickpt_share: linked %s to %s\n",
	        ickpt_file.c_str(),
	        hash_file.c_str());
	return true;
}

void
ickpt_share_init_sharing(const std::string& owner,
                         const std::string& hash,
                         const std::string& ickpt_file)
{
	std::string hash_file = make_hash_filename(owner, hash);
	if (link(ickpt_file.c_str(), hash_file.c_str()) == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: unexpected error linking %s to %s: %s\n",
		        hash_file.c_str(),
		        ickpt_file.c_str(),
		        strerror(errno));
		return;
	}
	dprintf(D_FULLDEBUG,
	        "ickpt_share: linked %s to %s\n",
	        hash_file.c_str(),
	        ickpt_file.c_str());
}

void
ickpt_share_try_removal(const std::string& owner, const std::string& hash)
{
	std::string hash_file = make_hash_filename(owner, hash);
	int count = link_count(hash_file.c_str());
	if (count == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: link_count error on %s; will not remove\n",
		        hash_file.c_str());
		return;
	}
	if (count != 1) {
		dprintf(D_FULLDEBUG,
		        "ickpt_share: link count for %s at %d\n",
		        hash_file.c_str(),
		        count);
		return;
	}
	if (unlink(hash_file.c_str()) == -1) {
		dprintf(D_ALWAYS,
		        "ickpt_share: unexpected unlink error on %s: %s\n",
		        hash_file.c_str(),
		        strerror(errno));
		return;
	}
	dprintf(D_FULLDEBUG, "ickpt_share: removed %s\n", hash_file.c_str());
}
