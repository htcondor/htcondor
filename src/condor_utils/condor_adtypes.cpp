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
#include "condor_adtypes.h"

AdTypes 
StringToAdType(const char *str)
{
	for (int i = 0; i < NUM_AD_TYPES; i++) {
		AdTypes type = static_cast<AdTypes>(i);
		if (strcasecmp(str,AdTypeToString(type)) == MATCH) {
			return type;
		}
	}
	return NO_AD;
}

const char*
AdTypeToString( AdTypes type )
{
	// ad type names indexed by *_AD enum
	static const char* const adtypes[NUM_AD_TYPES] = {
		STARTD_OLD_ADTYPE,	//STARTD_AD
		SCHEDD_ADTYPE,		//SCHEDD_AD
		MASTER_ADTYPE,		//MASTER_AD
		GATEWAY_ADTYPE,		//GATEWAY_AD
		CKPT_SRVR_ADTYPE,	//CKPT_SRVR_AD
		STARTD_PVT_ADTYPE,	//STARTD_PVT_AD /* no longer used */
		SUBMITTER_ADTYPE,	//SUBMITTOR_AD
		COLLECTOR_ADTYPE,	//COLLECTOR_AD
		LICENSE_ADTYPE,		//LICENSE_AD
		STORAGE_ADTYPE,		//STORAGE_AD
		ANY_ADTYPE,			//ANY_AD = 10
		BOGUS_ADTYPE,		//BOGUS_AD
		CLUSTER_ADTYPE,		//CLUSTER_AD
		NEGOTIATOR_ADTYPE,	//NEGOTIATOR_AD
		HAD_ADTYPE,			//HAD_AD
		GENERIC_ADTYPE,		//GENERIC_AD
		CREDD_ADTYPE,		//CREDD_AD
		DATABASE_ADTYPE,	//DATABASE_AD
		TT_ADTYPE,			//TT_AD
		GRID_ADTYPE, 		//GRID_AD
		XFER_SERVICE_ADTYPE,//XFER_SERVICE_AD	/* No longer used */
		LEASE_MANAGER_ADTYPE,//LEASE_MANAGER_AD	/* No longer used */
		DEFRAG_ADTYPE,		//DEFRAG_AD
		ACCOUNTING_ADTYPE,	//ACCOUNTING_AD = 23
		STARTD_SLOT_ADTYPE,	//SLOT_AD
		STARTD_DAEMON_ADTYPE,//STARTDAEMON_AD
	};

	if (type >= STARTD_AD  &&  type < NUM_AD_TYPES) {
		return adtypes[type];
	} else {
		return "Unknown";
	}
}

