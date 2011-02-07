/***************************************************************
 *
 * Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_cron_job_mode.h"

typedef CronJobModeTableEntry	TableEntry;

static const CronJobModeTableEntry mode_table[] = {
	CronJobModeTableEntry( CRON_WAIT_FOR_EXIT, true, "WaitForExit" ),
	CronJobModeTableEntry( CRON_PERIODIC, true, "Periodic" ),
	CronJobModeTableEntry( CRON_ONE_SHOT, true, "OneShot" ),
	CronJobModeTableEntry( CRON_ON_DEMAND, true, "OnDemand" ),
	CronJobModeTableEntry( CRON_ILLEGAL, false, "Illegal" )
};


CronJobModeTableEntry::CronJobModeTableEntry( CronJobMode mode,
											  bool valid,
											  const char *name )
		: m_mode( mode ),
		  m_name( name ),
		  m_valid( valid )
{
}

const CronJobModeTableEntry *
CronJobModeTable::Find( const char * s ) const
{
	const TableEntry *ent;
	for ( ent = mode_table;  *ent != CRON_ILLEGAL;  ent++ ) {
		if ( *ent == s ) {
			return ent;
		}
	}
	return NULL;
}

const CronJobModeTableEntry *
CronJobModeTable::Find( CronJobMode mode ) const
{
	const TableEntry *ent;
	for ( ent = mode_table;  *ent != CRON_ILLEGAL;  ent++ ) {
		if ( *ent == mode ) {
			return ent;
		}
	}
	return NULL;
}

static CronJobModeTable table;

const CronJobModeTable &
GetCronJobModeTable( void )
{
	return table;
}
