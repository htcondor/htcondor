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

/** Given a command/signal name, return the number. */
int getCommandNum( const char* );

/** Given a collector command number, return the (static buffer) string */
const char* getCollectorCommandString( int );

/** Given a collector command/signal name, return the number. */
int getCollectorCommandNum( const char* );

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
} CAResult;

const char* getCAResultString( CAResult r );
CAResult getCAResultNum( const char* str );

#endif
