/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
