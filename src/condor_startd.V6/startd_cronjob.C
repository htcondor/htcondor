/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
