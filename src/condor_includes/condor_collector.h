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

#ifndef __COLLECTOR_H__
#define __COLLECTOR_H__


enum AdTypes
{
	QUILL_AD,
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
	NUM_AD_TYPES,
	CLUSTER_AD,
	NEGOTIATOR_AD,
	HAD_AD,
	GENERIC_AD,
	CREDD_AD,
	DATABASE_AD,
	DBMSD_AD,
	TT_AD
};

#include "condor_commands.h"   // collector commands

#endif // __COLLECTOR_H__


