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

#ifndef _SCHEDD_CRONMGR_H
#define _SCHEDD_CRONMGR_H

#include "condor_cronmgr.h"

// Define a simple class to run child tasks periodically.
class ScheddCronMgr : public CronMgrBase
{
  public:
	ScheddCronMgr( void );
	virtual ~ScheddCronMgr( void );
	int Shutdown( bool force );
	bool ShutdownOk( void );

  protected:
	virtual CronJobBase *NewJob( const char *name );

  private:
	bool ShuttingDown;
	void JobEvent( CronJobBase *job, CondorCronEvent event );
	
};

#endif /* _SCHEDD_CRONMGR_H */
