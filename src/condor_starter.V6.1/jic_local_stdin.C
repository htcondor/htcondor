/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_string.h"
#include "condor_attributes.h"

#include "jic_local_stdin.h"


JICLocalStdin::JICLocalStdin( const char* keyword, int cluster, 
							  int proc, int subproc )
	: JICLocalConfig( keyword, cluster, proc, subproc )
{
		// nothing else to do, just let the appropriate JICLocalConfig
		// constructor do its thing
}


JICLocalStdin::JICLocalStdin( int cluster, int proc, int subproc )
	: JICLocalConfig( cluster, proc, subproc )
{
		// nothing else to do, just let the appropriate JICLocalConfig
		// constructor do its thing
}


JICLocalStdin::~JICLocalStdin()
{
		// nothing special
}


bool
JICLocalStdin::getLocalJobAd( void )
{ 
	bool found_some = false;
	dprintf( D_ALWAYS, "Reading job ClassAd from STDIN\n" );

	if( ! readStdinClassAd() ) {
		dprintf( D_ALWAYS, "No ClassAd data on STDIN\n" );
	} else { 
		dprintf( D_ALWAYS, "Found ClassAd data on STDIN\n" );
		found_some = true;
	}

		// if we weren't told on the command-line, see if there's a
		// keyword in the ClassAd we were given...
	if( key ) {
		dprintf( D_ALWAYS, "Job keyword \"%s\" specified on command-line\n", 
				 key );
	} else { 
		if( job_ad->LookupString(ATTR_JOB_KEYWORD, &key) )
			dprintf( D_ALWAYS, "Job keyword \"%s\" specified in ClassAd\n", 
					 key );
	}

	if( key ) {
			// if there's a keyword, try to get any attributes we may
			// not yet have from the config file
		return JICLocalConfig::getLocalJobAd();
	}

	return found_some;
}


bool
JICLocalStdin::readStdinClassAd( void ) 
{
	bool read_something = false;

    char buf[1024];
	while( fgets(buf, 1024, stdin) ) {
        read_something = true;
		chomp( buf );
		if( DebugFlags & D_JOB ) {
			dprintf( D_JOB, "STDIN: %s\n", buf );
		} 
        if( ! job_ad->Insert(buf) ) {
            dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
                     "ignoring this line\n", buf );
        }
    }

	return read_something;
}


bool
JICLocalStdin::getUniverse( void )
{
	int univ;

		// first, see if we've already got it in our ad
	if( job_ad->LookupInteger(ATTR_JOB_UNIVERSE, univ) ) {
		return checkUniverse( univ );
	}

		// now, try the config file...
	return JICLocalConfig::getUniverse();
} 
