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

#ifndef _ICKPT_SHARE_H
#define _ICKPT_SHARE_H

// This file encapsulates the SchedD's ability to avoid unneeded
// duplicates of "ickpt" files, which are copied into the SPOOL
// directory for jobs where copy_to_spool is true; this is done
// with a pretty basic mechanism based on the magic of hard links

#include <string>

#include "condor_common.h"
#include "condor_classad.h"

// given a ClassAd with information about a job's executable (like
// its MD5 sum), return a string that can be used as a per-owner
// handle for the executable. if an empty string is returned, sharing
// is disabled
//
std::string ickpt_share_get_hash(ClassAd& ad);

// for the given owner and hash, see if we already have a copy of
// a matching executable in SPOOL. if we do, link the given filename
// to the copy we already have and return true. otherwise, return false
//
bool ickpt_share_try_sharing(const std::string& owner,
                             const std::string& hash,
                             const std::string& ickpt_file);

// for the given owner and hash, make the matching executable
// ickpt_file available for sharing. this will be called when
// a new ickpt file has been placed in SPOOL
//
void ickpt_share_init_sharing(const std::string& owner,
                              const std::string& hash,
                              const std::string& ickpt_file);

// for the given owner and hash, see if any ickpt files are currently
// using the matching executable. if not, delete it
//
void ickpt_share_try_removal(const std::string& owner,
                             const std::string& hash);

#endif
