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


#ifndef COMMAND_STRINGS_H
#define COMMAND_STRINGS_H

#include "condor_common.h"
#include "condor_commands.h"
#include "translation_utils.h"
#include "stl_string_utils.h"
#include <array>

/* This file contains a mapping from Commands and Signals in 
   condor to the appropriate strings.  Some supporting functions
   are included.  A future implementation should use a hashtable,
   though we might not always want to incur the cost of hashing all
   the command names if we're not going to do lots of lookups.
*/

/** @name Command and Signal mapping

	Maps commands and signals from name to number and vice-versa.
*/

/** Given a command number, return the (static buffer) string */
const char* getCommandString( int );

/** Given a command number, always return a string suitable for printf */
const char* getCommandStringSafe(int num);

/** Given a command number return the string "command %d"
    the returned string is valid forever, and should not be freed by the caller
    it is not recommended to use this functuion unless getCommandString returns null
*/
const char* getUnknownCommandString(int num);


/** Given a command/signal name, return the number. */
int getCommandNum( const char* );

/** Given a collector command number, return the (static buffer) string */
const char* getCollectorCommandString( int );

/** Given a collector command/signal name, return the number. */
int getCollectorCommandNum( const char* );

/** Given an authz level, return an example command needing that authorization.
 *  This is useful for performing condor_ping where we need to ask about a specific
 *  command.
 */
int getSampleCommand( int authz_level );

/** All the possible results of a ClassAd command */  

typedef enum { 
	CA_SUCCESS = 1,
	CA_FAILURE,
	CA_NOT_AUTHENTICATED,
	CA_NOT_AUTHORIZED,
	CA_INVALID_REQUEST,
	CA_INVALID_STATE,
	CA_INVALID_REPLY,
	CA_LOCATE_FAILED,
	CA_CONNECT_FAILED,
	CA_COMMUNICATION_ERROR,
	CA_UNKNOWN_ERROR,
} CAResult;


// Return a std::array at compile time that other
// consteval functions can use as a lookup table
constexpr 
std::array<std::pair<const char *, CAResult>,11>
makeCATable() {
	return {{ // yes, there needs to be 2 open braces here...
		{ "Success", CA_SUCCESS },
		{ "Failure", CA_FAILURE },
		{ "NotAuthenticated", CA_NOT_AUTHENTICATED },
		{ "NotAuthorized", CA_NOT_AUTHORIZED },
		{ "InvalidRequest", CA_INVALID_REQUEST },
		{ "InvalidState", CA_INVALID_STATE },
		{ "InvalidReply", CA_INVALID_REPLY },
		{ "LocateFailed", CA_LOCATE_FAILED },
		{ "ConnectFailed", CA_CONNECT_FAILED },
		{ "CommunicationError", CA_COMMUNICATION_ERROR },
		{ "UnknownError", CA_UNKNOWN_ERROR },
	}};
}

// Almost all uses of this pass in a compile
// time constant argument, so make it consteval
// to do the lookup at compile time
constexpr 
const char* getCAResultString( CAResult r) {
	for (auto &[str, e]: makeCATable()) {
		if (e == r)  return str;
	}

	return nullptr;
}

constexpr
CAResult getCAResultNum(const char* instr) {
	for (auto &[str, e]: makeCATable()) {
		if (istring_view(str) == istring_view(instr)) return e;
	}
	return CA_UNKNOWN_ERROR;
}

constexpr 
std::array<std::pair<const char *,int>,3>
makeDrainTable() {
	return {{
		{ "graceful",	DRAIN_GRACEFUL},
		{ "quick", 		DRAIN_QUICK},
		{ "fast", 		DRAIN_FAST},
	}};	
}

constexpr
int getDrainingScheduleNum(char const *name) {
	for (auto &[str, e]: makeDrainTable()) {
		if (istring_view(str) == istring_view(name)) return e;
	}
	return -1;
}

/* Turn out this one is never called
constexpr
char const *getDrainingScheduleName( int num ) {
	for (auto &[str, e]: makeDrainTable()) {
		if (e == num)  return str;
	}
	return nullptr;
}
*/

#endif
