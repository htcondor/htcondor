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
	const char	*Name;			// Logical name
	const char	*ParamBase;		// Used for base of calls to param()
	const char	*configValProg;	// Config val program to run

	// Private member functions
	int DoConfig( bool initial = false );
	int ParseJobList( const char *JobListString );
	int ParseOldJobList( const char *JobListString );
	char *NextTok( char *cur, const char *tok );
	char *GetParam( const char *paramName, 
					const char *paramName2 = NULL );

};

#endif /* _CONDOR_CRONMGR_H */
