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


#ifndef _CONDOR_PERMS_H_
#define _CONDOR_PERMS_H_

#include "condor_common.h"

// IMPORTANT NOTE:  If you add a new enum value here, please add a new
// case to PermString() in condor_perms.c (in the util lib).
// There should be NO HOLES in the numerical values of this list,
// because NEXT_PERM() assumes values are contiguous.
// Also make any necessary updates to DCpermissionHierarchy.
// Be sure to leave "LAST_PERM" last.  It's a place holder so 
// we can iterate through all the permission levels, and know when to 
// stop. 
/// enum for Daemon Core socket/command/signal permissions
typedef enum {
  /** Place holder, must be same as next */  FIRST_PERM = 0,
  /** Open to everyone */                    ALLOW = 0,
  /** Able to read data */                   READ,
  /** Able to modify data (submit jobs) */   WRITE,
  /** From the negotiator */                 NEGOTIATOR,
  /** Administrative cmds (on, off, etc) */  ADMINISTRATOR,
  /** The machine owner (vacate) */          OWNER,
  /** Changing config settings remotely */   CONFIG_PERM,
  /** Daemon to daemon communcation     */   DAEMON,
  /** SOAP interface (http PUT) */			 SOAP_PERM,
  /** DEFAULT */                             DEFAULT_PERM,
  /** CLIENT */                              CLIENT_PERM,
  /** startd ad */                           ADVERTISE_STARTD_PERM,
  /** schedd ad */                           ADVERTISE_SCHEDD_PERM,
  /** master ad */                           ADVERTISE_MASTER_PERM,
  /** Place holder, must be last */          LAST_PERM,
  /** Perm value is unset*/                  UNSET_PERM
} DCpermission;

// convenience macro for iterating through DCpermission values
#define NEXT_PERM(perm) ( (DCpermission) (((int)perm)+1) )

BEGIN_C_DECLS
	
	/** PermString() converts the given DCpermission into the
		human-readable string version of the name.
		@param perm The permission you want to convert
		@return The string version of it
	*/
const char* PermString( DCpermission perm );

DCpermission getPermissionFromString( const char * permstring );

END_C_DECLS

#if defined(__cplusplus)

class DCpermissionHierarchy {

private:
	DCpermission m_base_perm; // the specified permission level

		// [0] is base perm, [1] is implied by [0], ...
		// Terminated by an entry with value LAST_PERM.
	DCpermission m_implied_perms[LAST_PERM+1];

		// List of perms that imply base perm, not including base perm,
		// and not including things that indirectly imply base perm, such
		// as the things that imply the members of this list.
		// Example: for base perm WRITE, this list includes DAEMON and
		// ADMINISTRATOR.
		// Terminated by an entry with value LAST_PERM.
	DCpermission m_directly_implied_by_perms[LAST_PERM+1];

		// [0] is base perm, [1] is perm to param for if [0] is undefined, ...
		// The list ends with DEFAULT_PERM, followed by LAST_PERM.
	DCpermission m_config_perms[LAST_PERM+1];

public:

		// [0] is base perm, [1] is implied by [0], ...
		// Terminated by an entry with value LAST_PERM.
	DCpermission const * getImpliedPerms() const { return m_implied_perms; }

		// List of perms that imply base perm, not including base perm,
		// and not including things that indirectly imply base perm, such
		// as the things that imply the members of this list.
		// Example: for base perm WRITE, this list includes DAEMON and
		// ADMINISTRATOR.
		// Terminated by an entry with value LAST_PERM.
	DCpermission const * getPermsIAmDirectlyImpliedBy() const { return m_directly_implied_by_perms; }

		// [0] is base perm, [1] is perm to param for if [0] is undefined, ...
		// The list ends with DEFAULT_PERM, followed by LAST_PERM.
	DCpermission const * getConfigPerms() const { return m_config_perms; }

	DCpermissionHierarchy(DCpermission perm);
};

#endif


#endif /* _CONDOR_PERMS_H_ */

