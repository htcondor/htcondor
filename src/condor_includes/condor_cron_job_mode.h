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

#ifndef _CONDOR_CRON_JOB_MODE_H
#define _CONDOR_CRON_JOB_MODE_H

#include "condor_common.h"

// Job's "run" (when to restart) mode
typedef enum 
{ 
	CRON_WAIT_FOR_EXIT,		// Timing from job's exit
	CRON_PERIODIC, 			// Run it periodically
	CRON_ONE_SHOT,			// "One-shot" timer
	CRON_ON_DEMAND,			// Run job on demand
	CRON_ILLEGAL			// Illegal mode
} CronJobMode;

// Job mode table entry
class CronJobModeTableEntry
{
public:
	CronJobModeTableEntry( CronJobMode mode, bool valid, const char *name );
	bool operator == ( CronJobMode mode ) const { return mode == m_mode; };
	bool operator != ( CronJobMode mode ) const { return mode != m_mode; };
	bool operator == ( const char *s ) const {
		return m_valid && (!strcasecmp(s, m_name));
	};
	CronJobMode Mode( void ) const { return m_mode; };
	const char *Name( void ) const { return m_name; };
	bool IsValid( void ) const { return m_valid; };
private:
	CronJobMode	 m_mode;
	const char	*m_name;
	bool		 m_valid;
};

// Job mode table
class CronJobModeTable
{
public:
	CronJobModeTable( void ) { };
	~CronJobModeTable( void ) { };
	const CronJobModeTableEntry *Find( const char *s ) const;
	const CronJobModeTableEntry *Find( CronJobMode ) const;
private:
};

extern const CronJobModeTable & GetCronJobModeTable( void );

#endif /* _CONDOR_CRON_JOB_MODE_H */
