/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"
#include "condor_classad.h"
#include "user_proc.h"
#include "starter.h"
#include "condor_attributes.h"
#include "classad_helpers.h"
#include "sig_name.h"

extern CStarter *Starter;


/* UserProc class implementation */

UserProc::~UserProc()
{
	if( name ) {
		free( name );
	}
}


void
UserProc::initialize( void )
{
	JobPid = -1;
	exit_status = -1;
	requested_exit = false;
	if( JobAd ) {
		initKillSigs();
	}
}


void
UserProc::initKillSigs( void )
{
	int sig;

	sig = findSoftKillSig( JobAd );
	if( sig >= 0 ) {
		soft_kill_sig = sig;
	} else {
		soft_kill_sig = SIGTERM;
	}

	sig = findRmKillSig( JobAd );
	if( sig >= 0 ) {
		rm_kill_sig = sig;
	} else {
		rm_kill_sig = SIGTERM;
	}

	const char* tmp = signalName( soft_kill_sig );
	dprintf( D_FULLDEBUG, "%s KillSignal: %d (%s)\n", 
			 name ? name : "Main job", soft_kill_sig, 
			 tmp ? tmp : "Unknown" );

	tmp = signalName( rm_kill_sig );
	dprintf( D_FULLDEBUG, "%s RmKillSignal: %d (%s)\n", 
			 name ? name : "Main job", rm_kill_sig, 
			 tmp ? tmp : "Unknown" );
}


bool
UserProc::PublishUpdateAd( ClassAd* ad )
{
	char buf[256];

	if( JobPid >= 0 ) { 
		sprintf( buf, "%s%s=%d", name ? name : "", ATTR_JOB_PID,
				 JobPid );
		ad->Insert( buf );
	}

	if( job_start_time.seconds() > 0 ) {
		sprintf( buf, "%s%s=%ld", name ? name : "", ATTR_JOB_START_DATE,
				 job_start_time.seconds() );
		ad->Insert( buf );
	}

	if( exit_status >= 0 ) {

		if( job_exit_time.seconds() > 0 ) {
			sprintf( buf, "%s%s=%f", name ? name : "", ATTR_JOB_DURATION, 
					 job_exit_time.difference(&job_start_time) );
			ad->Insert( buf );
		}

			/*
			  If we have the exit status, we want to parse it and set
			  some attributes which describe the status in a platform
			  independent way.  This way, we're sure we're analyzing
			  the status integer with the platform-specific macros
			  where it came from, instead of assuming that WIFEXITED()
			  and friends will work correctly on a status integer we
			  got back from a different platform.
			*/
		if( WIFSIGNALED(exit_status) ) {
			sprintf( buf, "%s%s = TRUE", name ? name : "",
					 ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s%s = %d", name ? name : "",
					 ATTR_ON_EXIT_SIGNAL, WTERMSIG(exit_status) );
			ad->Insert( buf );
			sprintf( buf, "%s%s = \"died on %s\"",
					 name ? name : "", ATTR_EXIT_REASON,
					 daemonCore->GetExceptionString(WTERMSIG(exit_status)) );
			ad->Insert( buf );
		} else {
			sprintf( buf, "%s%s = FALSE", name ? name : "",
					 ATTR_ON_EXIT_BY_SIGNAL );
			ad->Insert( buf );
			sprintf( buf, "%s%s = %d", name ? name : "",
					 ATTR_ON_EXIT_CODE, WEXITSTATUS(exit_status) );
			ad->Insert( buf );
		}
	}
	return true;
}


void
UserProc::PublishToEnv( Env* proc_env )
{
	if( exit_status >= 0 ) {
			// TODO: what should these really be called?  use
			// myDistro?  mySubSystem?  hard to say...
		MyString base;
		base = "_";
		base += myDistro->Get();
		base += '_';
		if( name ) {
			base += name;
		} else {
			base += "MAINJOB";
		}
		base += '_';
		base.upper_case();
 
		MyString env_name;

		if( WIFSIGNALED(exit_status) ) {
			env_name = base.GetCStr();
			env_name += "EXIT_SIGNAL";
			proc_env->Put( env_name.GetCStr(), WTERMSIG(exit_status) );
		} else {
			env_name = base.GetCStr();
			env_name += "EXIT_CODE";
			proc_env->Put( env_name.GetCStr(), WEXITSTATUS(exit_status) );
		}
	}
}
