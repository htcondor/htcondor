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
#include "condor_open.h"
#include "condor_constants.h"
#include "condor_debug.h"
#include "directory.h"
#include "condor_string.h"
#include "status_string.h"
#include "condor_config.h"
#include "stat_wrapper.h"
#include "perm.h"
#include "my_username.h"
#include "my_popen.h"
#include "directory_util.h"
#include "filename_tools.h"

bool mkdir_and_parents_if_needed_cur_priv( const char *path, mode_t mode )
{
	int tries = 0;

		// There is a possible race condition here in which the parent
		// does not exist, so we create the parent, but before we can
		// create the child, something else calls rmdir on the parent.
		// The following code detects this condition and repeats until
		// successful.  To guard against some sort of pathological
		// condition that causes this to keep happening forever in a
		// tight loop, there is an upper bound on how many attempts
		// will be made.

	for(tries=0; tries < 100; tries++) {

			// Optimize for the case where parents already exist, so try
			// to create the directory first, before looking at parents.

		if( mkdir( path, mode ) == 0 ) {
			errno = 0; // tell caller that path did not already exist
			return true;
		}

		if( errno == EEXIST ) {
				// leave errno as is so caller can tell path existed
			return true;
		}
		if( errno != ENOENT ) {
				// we failed for a reason other than a missing parent dir
			return false;
		}

		std::string parent,junk;
		if( filename_split(path,parent,junk) ) {
			if(!mkdir_and_parents_if_needed_cur_priv( parent.c_str(),mode)) {
				return false;
			}
		}
	}

	dprintf(D_ALWAYS,
			"Failed to create %s after %d attempts.\n", path, tries);
	return false;
}

bool mkdir_and_parents_if_needed( const char *path, mode_t mode, priv_state priv )
{
	bool retval;
	priv_state saved_priv;

	if( priv != PRIV_UNKNOWN ) {
		saved_priv = set_priv(priv);
	}

	retval = mkdir_and_parents_if_needed_cur_priv(path,mode);

	if( priv != PRIV_UNKNOWN ) {
		set_priv(saved_priv);
	}
	return retval;
}

bool make_parents_if_needed( const char *path, mode_t mode, priv_state priv )
{
	std::string parent,junk;

	ASSERT( path );

	filename_split(path,parent,junk);
	return mkdir_and_parents_if_needed( parent.c_str(), mode, priv );
}
