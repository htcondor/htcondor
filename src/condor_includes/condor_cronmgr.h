/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_CRONMGR_H
#define _CONDOR_CRONMGR_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_cron.h"

// Define a simple class to run child tasks periodically.
class CondorCronMgr
{
  public:
	CondorCronMgr( const char *name );
	virtual ~CondorCronMgr( );
	int Reconfig( void );
	int KillAll( void );

  protected:
	virtual CondorCronJob *NewJob( const char *name );
	int SetName( const char *name, 
				 const char *setParamBase = NULL,
				 const char *setParamExt = NULL );
	int SetParamBase( const char *base, const char *ext );

  private:
	CondorCron	Cron;
	const char	*Name;			// Logical name
	const char	*ParamBase;		// Used for base of calls to param()
	const char	*Args;

	// Private member functions
	int ParseJobList( const char *JobListString );
	char *GetParam( const char *paramName, 
					const char *paramName2 = NULL );
};

#endif /* _CONDOR_CRONMGR_H */
