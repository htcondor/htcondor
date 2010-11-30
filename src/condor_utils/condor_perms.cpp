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
	case NEGOTIATOR:
		return "NEGOTIATOR";
	case ADMINISTRATOR:
		return "ADMINISTRATOR";
	case OWNER:
		return "OWNER";
	case CONFIG_PERM:
		return "CONFIG";
    case DAEMON:
        return "DAEMON";
	case SOAP_PERM:
		return "SOAP";
	case DEFAULT_PERM:
		return "DEFAULT";
	case CLIENT_PERM:
		return "CLIENT";
	case ADVERTISE_STARTD_PERM:
		return "ADVERTISE_STARTD";
	case ADVERTISE_SCHEDD_PERM:
		return "ADVERTISE_SCHEDD";
	case ADVERTISE_MASTER_PERM:
		return "ADVERTISE_MASTER";
	default:
		return "Unknown";
	}
	return "Unknown";
};

