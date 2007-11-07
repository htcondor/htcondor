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

#ifndef _CONDOR_CRONMGR_H
#define _CONDOR_CRONMGR_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_cron.h"

// Define a simple class to run child tasks periodically.
class CronMgrBase : public Service
{
  public:
	CronMgrBase( const char *name );
	virtual ~CronMgrBase( );
	virtual int Initialize( void );
	virtual int Reconfig( void );
	int KillAll( bool force );
	bool IsAllIdle( void );

  protected:
	virtual CronJobBase *NewJob( const char *name ) = 0;
	int SetName( const char *name, 
				 const char *setParamBase = NULL,
				 const char *setParamExt = NULL );
	int SetParamBase( const char *base, const char *ext );
	const char *GetName( void ) { return Name; };

  private:
	CondorCron	Cron;
	char	*Name;			// Logical name
	char	*ParamBase;		// Used for base of calls to param()
	char	*configValProg;	// Config val program to run

	// Private member functions
	int DoConfig( bool initial = false );
	int ParseJobList( const char *JobListString );
	int ParseOldJobList( const char *JobListString );
	char *NextTok( char *cur, const char *tok );
	char *GetParam( const char *paramName, 
					const char *paramName2 = NULL );

};

#endif /* _CONDOR_CRONMGR_H */
