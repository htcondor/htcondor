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


#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_attributes.h"

#include "jic_local_file.h"


JICLocalFile::JICLocalFile( const char* classad_filename,
							const char* keyword, 
							int cluster, int proc, int subproc )
	: JICLocalConfig( keyword, cluster, proc, subproc )
{
	initFilenames( classad_filename );
}


JICLocalFile::JICLocalFile( const char* classad_filename, int cluster, int proc, int subproc )
	: JICLocalConfig( cluster, proc, subproc )
{
	initFilenames( classad_filename );
}


JICLocalFile::~JICLocalFile()
{
	if( job_filename ) {
		free( job_filename );
	}
}


void
JICLocalFile::initFilenames( const char* jobad_path )
{
	if( ! jobad_path ) {
		EXCEPT( "Can't instantiate a JICLocalFile without a filename!" );
	}
	if( jobad_path[0] == '-' && jobad_path[1] == '\0' ) {
			// special case, treat '-' as STDIN
		job_filename = NULL;
	} else {
		job_filename = strdup( jobad_path );
	}
}


bool
JICLocalFile::getLocalJobAd( void )
{ 
	bool found_some = false;
	dprintf( D_ALWAYS, "Reading job ClassAd from \"%s\"\n", jobAdFileName() );

	if( ! readClassAdFromFile( job_filename, job_ad ) ) {
		dprintf( D_ALWAYS, "No ClassAd data in \"%s\"\n", jobAdFileName() );
	} else { 
		dprintf( D_ALWAYS, "Found ClassAd data in \"%s\"\n", jobAdFileName() );
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


const char*
JICLocalFile::jobAdFileName( void )
{
	if( job_filename ) {
		return job_filename;
	}
	return "STDIN";
} 


bool
JICLocalFile::readClassAdFromFile( char* filename, ClassAd* ad ) 
{
	bool read_something = false;
	bool needs_close = true;
	FILE* fp;

	if( filename ) {
		fp = safe_fopen_wrapper_follow( filename, "r" );
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

	std::string line;
	while( readLine(line, fp) ) {
        read_something = true;
		chomp(line);
		if( line[0] == '#' ) {
			dprintf( D_JOB, "IGNORING COMMENT: %s\n", line.c_str() );
			continue;
		}
		if( ! line[0] ) {
				// ignore blank lines
			continue;
		}
		if( IsDebugLevel( D_JOB ) ) {
			dprintf( D_JOB, "FILE: %s\n", line.c_str() );
		} 
        if( ! ad->Insert(line) ) {
            dprintf( D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
                     "ignoring this line\n", line.c_str() );
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
