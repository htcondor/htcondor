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
#include <vector>

// IMPORTANT NOTE:  If you add a new enum value here, please update
// makePermStringTable() in condor_perms.cpp
// There should be NO HOLES in the numerical values of this list,
// because NEXT_PERM() assumes values are contiguous.
// Also make any necessary updates to DCpermissionHierarchy.
// Be sure to leave "LAST_PERM" last.  It's a place holder so 
// we can iterate through all the permission levels, and know when to 
// stop. 
/// enum for Daemon Core socket/command/signal permissions
enum DCpermission {
	                                         NOT_A_PERM = -1,
  /** Place holder, must be same as next */  FIRST_PERM = 0,
  /** Open to everyone */                    ALLOW_PERM = 0,     ALLOW=ALLOW_PERM,
  /** Able to read data */                   READ_PERM,          READ=READ_PERM,
  /** Able to modify data (submit jobs) */   WRITE_PERM,         WRITE=WRITE_PERM,
  /** From the negotiator */                 NEGOTIATOR_PERM,    NEGOTIATOR=NEGOTIATOR_PERM,
  /** Administrative cmds (on, off, etc) */  ADMINISTRATOR_PERM, ADMINISTRATOR=ADMINISTRATOR_PERM,
  /** Changing config settings remotely */   CONFIG_PERM,
  /** Daemon to daemon communcation     */   DAEMON_PERM,        DAEMON=DAEMON_PERM,
  /** SOAP interface (http PUT) */			 SOAP_PERM,
  /** DEFAULT */                             DEFAULT_PERM,
  /** CLIENT */                              CLIENT_PERM,
  /** startd ad */                           ADVERTISE_STARTD_PERM,
  /** schedd ad */                           ADVERTISE_SCHEDD_PERM,
  /** master ad */                           ADVERTISE_MASTER_PERM,
  /** Place holder, must be last */          LAST_PERM,
  /** Perm value is unset*/                  UNSET_PERM
};

// convenience macro for iterating through DCpermission values
#define NEXT_PERM(perm) ( (DCpermission) (((int)perm)+1) )

	/** PermString() converts the given DCpermission into the
		human-readable string version of the name.
		@param perm The permission you want to convert
		@return The string version of it
	*/
const char* PermString( DCpermission perm );
const char* PermDescription( DCpermission perm );

DCpermission getPermissionFromString( const char * permstring );

class DCpermissionHierarchy {
private:
	static const DCpermission aConfigNext[LAST_PERM+1];
	static const DCpermission aConfigNextLegacy[LAST_PERM+1];
	static const DCpermission aImpliedNext[LAST_PERM+1];
public:
	DCpermissionHierarchy() = delete;
	~DCpermissionHierarchy() = delete;

	static DCpermission nextConfig(DCpermission perm, bool legacy_allow_semantics) {
		if (perm < FIRST_PERM || perm >= LAST_PERM) return UNSET_PERM;
		if (legacy_allow_semantics) { return aConfigNextLegacy[perm]; }
		return aConfigNext[perm];
	}
	static DCpermission nextImplied(DCpermission perm) {
		if (perm < FIRST_PERM || perm >= LAST_PERM) return UNSET_PERM;
		return aImpliedNext[perm];
	}

	static std::vector<DCpermission> DirectlyImpliedBy(DCpermission perm) {
		std::vector<DCpermission> ret;
		for (int pm = FIRST_PERM; pm < LAST_PERM; ++pm) {
			if (aImpliedNext[pm] == perm) ret.push_back((DCpermission)(pm));
		}
		return ret;
	}
};

#endif /* _CONDOR_PERMS_H_ */

