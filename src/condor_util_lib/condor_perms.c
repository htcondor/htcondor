/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "condor_perms.h"

const char*
PermString( DCpermission perm )
{
	switch( perm ) {
	case ALLOW:
		return "ALLOW";
	case READ:
		return "READ";
	case WRITE:
		return "WRITE";
    case DAEMON:
        return "DAEMON";
	case NEGOTIATOR:
		return "NEGOTIATOR";
	case IMMEDIATE_FAMILY:
		return "IMMEDIATE_FAMILY";
	case ADMINISTRATOR:
		return "ADMINISTRATOR";
	case OWNER:
		return "OWNER";
	case CONFIG_PERM:
		return "CONFIG";
	case SOAP_PERM:
		return "SOAP";
	default:
		return "Unknown";
	}
	return "Unknown";
};

