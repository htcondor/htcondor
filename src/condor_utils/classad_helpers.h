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

#ifndef __CLASSAD_HELPERS_H_
#define __CLASSAD_HELPERS_H_

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

// same as findSoftKillSig(), but for ATTR_CHECKPOINT_SIG
int findCheckpointSig( ClassAd* ad );

// Based on info in the ClassAd and the given exit reason, construct
// the appropriate string describing the fate of the job...
bool printExitString( ClassAd* ad, int exit_reason, std::string &str );

// Remove/replace characters from the string so it can be used as an attribute name
// it changes the string that is passed to it.  first leading an trailing spaces
// are removed, then Characters that are invalid in compatible classads 
// (basically anthing but [a-zA-Z0-9_]) is replaced with chReplace.
// if chReplace is 0, then invalid characters are removed. 
// if compact is true, then multiple consecutive runs of chReplace
// are changed to a single instance.
// return value is the length of the resulting string.
// NOTE: This function does not ensure that the first character isn't
//   a digit. For current callers, this is not a problem.
int cleanStringForUseAsAttr(std::string &str, char chReplace=0, bool compact=true);

// Create an empty job ad, with sensible defaults for all of the attributes
// that the schedd expects to be set, like condor_submit would set them.
// owner, universe, and cmd are the only attributes that require an
// explicit value. If NULL is passed for owner, the attribute is explicitly
// set to Undefined, which tells the schedd to fill in the attribute. This
// feature is only used by the python bindings currently.
// The caller is responible for calling 'delete' on the returned ClassAd.
ClassAd *CreateJobAd( const char *owner, int universe, const char *cmd );

// tokenize the input string, and insert tokens into the attrs set
bool add_attrs_from_string_tokens(classad::References & attrs, const char * str, const char * delims=NULL);
inline bool add_attrs_from_string_tokens(classad::References & attrs, const std::string & str, const char * delims=NULL) {
	if (str.empty()) return false;
	return add_attrs_from_string_tokens(attrs, str.c_str(), delims);
}

// print attributes to a std::string, returning the result as a const char *
const char *print_attrs(std::string &out, bool append, const classad::References & attrs, const char * delim);


#endif
