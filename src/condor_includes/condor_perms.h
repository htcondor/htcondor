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
  /** SOAP interface (http PUT) */			 SOAP_PERM = 9,
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

