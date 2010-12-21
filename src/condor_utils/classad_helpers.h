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

 
/*
  This file holds utility functions that rely on ClassAds.
*/

#include "condor_attributes.h"

/*
  lookup ATTR_KILL_SIG, but if it's a string represenation, convert it
  to the appropriate signal number for the local platform.
*/
int findSoftKillSig( ClassAd* ad );

// same as findSoftKillSig(), but for ATTR_REMOVE_KILL_SIG
int findRmKillSig( ClassAd* ad );

// same as findSoftKillSig(), but for ATTR_HOLD_KILL_SIG
int findHoldKillSig( ClassAd* ad );

// Based on info in the ClassAd and the given exit reason, construct
// the appropriate string describing the fate of the job...
bool printExitString( ClassAd* ad, int exit_reason, MyString &str );

// Create an empty job ad, with sensible defaults for all of the attributes
// that the schedd expects to be set, like condor_submit would set them.
// owner, universe, and cmd are the only attributes that require an
// explicit value. If NULL is passed for owner, the attribute is explicitly
// set to Undefined, which tells the schedd to fill in the attribute. This
// feature is only used by the soap interface currently.
// The caller is responible for calling 'delete' on the returned ClassAd.
ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd );

/*
	This function tells the caller if a UserLog object should be
	constructed or not, and if so, says where the user wants the user
	log file to go. The difference between this function and simply
	doing a LookupString() on ATTR_ULOG_FILE is that A) the result is
	combined with IWD if necessary to form an absolute path, and B) if
	EVENT_LOG is defined in the condor_config file, then the result
	will be /dev/null even if ATTR_ULOG_FILE is not defined (since we
	still want a UserLog object in this case so the global event log
	is updated). Return function is true if ATTR_ULOG_FILE is found or
	if EVENT_LOG is defined, else false.
*/
bool getPathToUserLog(ClassAd *job_ad, MyString &result,
					   const char* ulog_path_attr = ATTR_ULOG_FILE);
