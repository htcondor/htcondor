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

#ifndef __CONDOR_ADTYPES_H__
#define __CONDOR_ADTYPES_H__

#define DATABASE_ADTYPE			"Database"
#define DEFRAG_ADTYPE			"Defrag"
#define TT_ADTYPE			"TTProcess"
#define STARTD_ADTYPE			"Machine"
#define STARTD_PVT_ADTYPE		"MachinePrivate"
#define SCHEDD_ADTYPE			"Scheduler"
#define MASTER_ADTYPE			"DaemonMaster"
#define CKPT_SRVR_ADTYPE		"CkptServer"
#define JOB_ADTYPE			"Job"
#define JOB_SET_ADTYPE			"JobSet"
#define QUERY_ADTYPE			"Query"
#define COLLECTOR_ADTYPE		"Collector"
#define CKPT_FILE_ADTYPE		"CkptFile"
#define USERAUTH_ADTYPE			"Authentication"
#define LICENSE_ADTYPE			"License"
#define STORAGE_ADTYPE			"Storage"
#define ACCOUNTING_ADTYPE			"Accounting"
#define NEGOTIATOR_ADTYPE		"Negotiator"
#define HAD_ADTYPE			"HAD"
#define XFER_SERVICE_ADTYPE		"XferService"		/* No longer used */
#define LEASE_MANAGER_ADTYPE		"LeaseManager"	/* No longer used */
#define CREDD_ADTYPE			"CredD"
#define ANY_ADTYPE			"Any"
#define GENERIC_ADTYPE			"Generic"
#define SUBMITTER_ADTYPE		"Submitter"
#define COMMAND_ADTYPE			"Command"
#define REPLY_ADTYPE			"Reply"
#define GATEWAY_ADTYPE			"Gateway"
#define CLUSTER_ADTYPE	 		"Cluster"
#define GRID_ADTYPE			"Grid"
#define BOGUS_ADTYPE		"Bogus"

// Enumerated list of ad types (for the query object)
enum AdTypes
{
	NO_AD = -1,
	STARTD_AD,
	SCHEDD_AD,
	MASTER_AD,
	GATEWAY_AD,
	CKPT_SRVR_AD,
	STARTD_PVT_AD,
	SUBMITTOR_AD,
	COLLECTOR_AD,
	LICENSE_AD,
	STORAGE_AD,
	ANY_AD,
	BOGUS_AD,		// placeholder: NUM_AD_TYPES used wrongly to be here
	CLUSTER_AD,
	NEGOTIATOR_AD,
	HAD_AD,
	GENERIC_AD,
	CREDD_AD,
	DATABASE_AD,
	TT_AD,
	GRID_AD,
	XFER_SERVICE_AD,	// placeholder: this type no longer used
	LEASE_MANAGER_AD,	// placeholder: this type no longer used
	DEFRAG_AD,
	ACCOUNTING_AD,
	// This should *ALWAYS* be at the end of this list
	NUM_AD_TYPES,
};

AdTypes
AdTypeFromString(const char* adtype_string) ;

const char*
AdTypeToString( AdTypes type );

#endif // __CONDOR_ADTYPES_H__
