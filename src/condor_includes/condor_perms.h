/**************************************************************
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright © 1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.	
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.	For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
**************************************************************/


#ifndef _CONDOR_PERMS_H_
#define _CONDOR_PERMS_H_

#include "condor_common.h"

// IMPORTANT NOTE:  If you add a new enum value here, please add a new
// case to PermString() in condor_perms.c (in the util lib).
// Also, be sure to leave "LAST_PERM" last.  It's a place holder so 
// we can iterate through all the permission levels, and know when to 
// stop. 
/// enum for Daemon Core socket/command/signal permissions
typedef enum {
  /** Open to everyone */                    ALLOW = 0,
  /** Able to read data */                   READ = 1,
  /** Able to modify data (submit jobs) */   WRITE = 2,
  /** From the negotiator */                 NEGOTIATOR = 3,
  /** Not yet implemented, do NOT use */     IMMEDIATE_FAMILY = 4,
  /** Administrative cmds (on, off, etc) */  ADMINISTRATOR = 5,
  /** The machine owner (vacate) */          OWNER = 6,
  /** Changing config settings remotely */   CONFIG_PERM = 7,
  /** Daemon to daemon communcation     */   DAEMON = 8,
  /** Place holder, must be last */          LAST_PERM
} DCpermission;


BEGIN_C_DECLS
	
	/** PermString() converts the given DCpermission into the
		human-readable string version of the name.
		@param perm The permission you want to convert
		@return The string version of it
	*/
const char* PermString( DCpermission perm );

END_C_DECLS


#endif /* _CONDOR_PERMS_H_ */

