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

#include "condor_common.h"
#include "startd_cronjob.h"
#include "startd.h"

// Interface version number
#define	INTERFACE_VERSION	"1"

// CronJob constructor
StartdCronJob::
StartdCronJob( const char *mgrName, const char *jobName ) :
		CondorCronJob( mgrName, jobName )
{
	// Register it with the Resource Manager
	resmgr->adlist_register( jobName );
	OutputAd = NULL;
	OutputAdCount = 0;

	// Build my interface version environment (but, I need a 'name' to do it)
	if ( mgrName && (*mgrName) ) {
		char	*nameUc = strdup( mgrName );
		char	*namePtr;
		for( namePtr = nameUc; *namePtr; namePtr++ ) {
			if ( islower ( *namePtr ) ) {
				*namePtr = toupper( *namePtr );
			}
		}
		EnvStr = nameUc;
		EnvStr += "_INTERFACE_VERSION=" INTERFACE_VERSION;
	}
}

// StartdCronJob destructor
StartdCronJob::
~StartdCronJob( )
{
	// Delete myself from the resource manager
	resmgr->adlist_delete( GetName() );
	if ( NULL != OutputAd ) {
		delete( OutputAd );
	}
}

// StartdCronJob initializer
int
StartdCronJob::Initialize( )
{
	AddEnv( EnvStr.GetCStr() );

	// And, run the "main" Initialize function
	return CondorCronJob::Initialize( );
}

// Process a line of input
int
StartdCronJob::
ProcessOutput( const char *line )
{
	if ( NULL == OutputAd ) {
		OutputAd = new ClassAd( );
	}

	// NULL line means end of list
	if ( NULL == line ) {
		// Publish it
		if ( OutputAdCount != 0 ) {

			// Insert the 'LastUpdate' field
			const char      *prefix = GetPrefix( );
			if ( prefix ) {
				MyString    Update( prefix );
				Update += "LastUpdate = ";
				char    tmpBuf [ 20 ];
				sprintf( tmpBuf, "%ld", (long) time( NULL ) );
				Update += tmpBuf;
				const char  *UpdateStr = Update.Value( );

				// Add it in
				if ( ! OutputAd->Insert( UpdateStr ) ) {
					dprintf( D_ALWAYS, "Can't insert '%s' into '%s' ClassAd\n",
							 UpdateStr, GetName() );
					// TodoWrite( );
				}
			}

			// Replace the old ClassAd now
			resmgr->adlist_replace( GetName( ), OutputAd );

			// I've handed it off; forget about it!
			OutputAd = NULL;
			OutputAdCount = 0;
		}
	} else {
		// Process this line!
		if ( ! OutputAd->Insert( line ) ) {
			dprintf( D_ALWAYS, "Can't insert '%s' into '%s' ClassAd\n",
					 line, GetName() );
			// TodoWrite( );
		} else {
			OutputAdCount++;
		}
	}
	return OutputAdCount;
}
