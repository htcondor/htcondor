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

#include "jic_local_file.h"


JICLocalFile::JICLocalFile( const char* classad_filename, 
							const char* keyword, 
							int cluster, int proc, int subproc )
	: JICLocalConfig( keyword, cluster, proc, subproc )
{
	initFilename( classad_filename );
}


JICLocalFile::JICLocalFile( const char* classad_filename, 
							int cluster, int proc, int subproc )
	: JICLocalConfig( cluster, proc, subproc )
{
	initFilename( classad_filename );
}


JICLocalFile::~JICLocalFile()
{
	if( filename ) {
		free( filename );
	}
}


void
JICLocalFile::initFilename( const char* path )
{
	if( ! path ) {
		EXCEPT( "Can't instantiate a JICLocalFile without a filename!" );
	}
	if( path[0] == '-' && path[1] == '\0' ) {
			// special case, treat '-' as STDIN
		filename = NULL;
	} else {
		filename = strdup( path );
	}
}


bool
JICLocalFile::getLocalJobAd( void )
{ 
	bool found_some = false;
	dprintf( D_ALWAYS, "Reading job ClassAd from \"%s\"\n", fileName() );

	if( ! readClassAdFromFile() ) {
		dprintf( D_ALWAYS, "No ClassAd data in \"%s\"\n", fileName() );
	} else { 
		dprintf( D_ALWAYS, "Found ClassAd data in \"%s\"\n", fileName() );
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


char*
JICLocalFile::fileName( void )
{
	if( filename ) {
		return filename;
	}
	return "STDIN";
} 


bool
JICLocalFile::readClassAdFromFile( void ) 
{
	bool read_something = false;
	bool needs_close = true;
	FILE* fp;

	if( filename ) {
		fp = fopen( filename, "r" );
		if( ! fp ) {
			dprintf( D_ALWAYS, "failed to open \"%s\" for reading: %s "
					 "(errno %d)\n", filename, strerror(errno), errno );
			return false;
		}
	} else {
			// this is the STDIN special-case, if we were passed '-'
			// on the command-line
		fp = stdin;
		needs_close = false;
	}

	MyString line;
	while( line.readLine(fp) ) {
        read_something = true;
		line.chomp();
		if( line[0] == '#' ) {
			dprintf( D_JOB, "IGNORING COMMENT: %s\n", line.GetCStr() );
			continue;
		}
		if( DebugFlags & D_JOB ) {
			dprintf( D_JOB, "FILE: %s\n", line.GetCStr() );
		} 
        if( ! job_ad->Insert(line.GetCStr()) ) {
            dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
                     "ignoring this line\n", line.GetCStr() );
        }
    }
	
	if( needs_close ) {
		fclose( fp );
	}
	return read_something;
}


bool
JICLocalFile::getUniverse( void )
{
	int univ;

		// first, see if we've already got it in our ad
	if( job_ad->LookupInteger(ATTR_JOB_UNIVERSE, univ) ) {
		return checkUniverse( univ );
	}

		// now, try the config file...
	return JICLocalConfig::getUniverse();
} 
