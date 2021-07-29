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
#include "domain_tools.h"

#include "jic_local_config.h"


JICLocalConfig::JICLocalConfig( const char* keyword, int cluster, 
								int proc, int subproc ) : JICLocal()
{
	if( keyword ) {
		key = strdup( keyword );
	} else {
		EXCEPT( "Can't instantiate a JICLocalConfig object without "
				"a keyword" );
	}
	job_cluster = cluster;
	job_proc = proc;
	job_subproc = subproc;
	job_ad = new ClassAd();
	mach_ad = new ClassAd();
}


JICLocalConfig::JICLocalConfig( int cluster, int proc, int subproc ) 
	: JICLocal()
{
		// this is the protected version that doesn't care if there's
		// no key...  
	key = NULL;
	job_cluster = cluster;
	job_proc = proc;
	job_subproc = subproc;
	job_ad = new ClassAd();
	mach_ad = new ClassAd();
}



JICLocalConfig::~JICLocalConfig()
{
	if( key ) {
		free( key );
	}
}


bool
JICLocalConfig::getLocalJobAd( void )
{ 
	if( ! key ) {
		dprintf( D_ALWAYS, "ERROR: Cannot get job ClassAd from config file "
				 "without a keyword, aborting\n" );
		return false;
	}

	dprintf( D_ALWAYS, "Getting job ClassAd from config file "
			 "with keyword: \"%s\"\n", key );

		// first, things we absolutely need
	if( ! getUniverse() ) { 
		return false;
	}
	if( ! getString( 1, ATTR_JOB_CMD, "executable") ) {
		return false;
	}
	if( ! getString( 1, ATTR_JOB_IWD, "initialdir") ) {
		return false;
	}

#if defined ( WIN32 )	
		// Windows "owners" may be of the form domain\user,
		// so we'll need to parse it...
	std::string buffer;
	formatstr( buffer, "%s_%s", key, ATTR_OWNER );
	char *owner_defined = param( buffer.c_str() );
	if ( owner_defined ) {			
			// On Windows we need to set RunAsOwner for it to 
			// respect the owner attribute (but only when the 
			// starter allows it).
		bool run_as_owner = param_boolean( 
			"STARTER_ALLOW_RUNAS_OWNER", false, true, NULL, job_ad );
		if ( run_as_owner ) {
				// Add the RunAsOwner attribute:
			job_ad->Assign( ATTR_JOB_RUNAS_OWNER, true );
				// Parse the OWNER attribute and add the new
				// OWNER and NTDOMAIN attributes:
			char const *local_domain = ".";
			char *name = NULL, *domain = NULL;
			getDomainAndName ( owner_defined, domain, name );
			job_ad->Assign( ATTR_OWNER, name );
			job_ad->Assign( ATTR_NT_DOMAIN, domain ? domain : local_domain );
		} else {
			dprintf( D_ALWAYS, "Local job \"Owner\" defined, "
				"but will not be used: please set "
				"STARTER_ALLOW_RUNAS_OWNER to True to "
				"enable this.\n" );
		}
		free( owner_defined );
	}		
#else
		// ATTR_OWNER only matters if we're running as root, and we'll
		// catch it later if we need it and it's not defined.  so,
		// just treat it as optional at this point.
	getString( 0, ATTR_OWNER, NULL );
#endif 

		// now, optional things
	getString( 0, ATTR_JOB_INPUT, "input" );
	getString( 0, ATTR_JOB_OUTPUT, "output" );
	getString( 0, ATTR_JOB_ERROR, "error" );
	getString( 0, ATTR_JOB_ARGUMENTS1, "arguments" );
	getString( 0, ATTR_JOB_ARGUMENTS2, "arguments2" );
	getString( 0, ATTR_JOB_ENVIRONMENT1, "environment" );
	getString( 0, ATTR_JOB_ENVIRONMENT2, "environment2" );
	getString( 0, ATTR_JAR_FILES, "jar_files" );
	getInt( 0, ATTR_KILL_SIG, "kill_sig" );
	getBool( 0, ATTR_STARTER_WAIT_FOR_DEBUG, "starter_wait_for_debug" );
	getString( 0, ATTR_STARTER_ULOG_FILE, "log" );
	getBool( 0, ATTR_STARTER_ULOG_USE_XML, "log_use_xml" );

		// only check for cluster and proc in the config file if we
		// didn't get them on the command-line
	if( job_cluster < 0 ) { 
		getInt( 0, ATTR_CLUSTER_ID, "cluster" );
	} else {
		job_ad->Assign( ATTR_CLUSTER_ID, job_cluster );
	}
	if( job_proc < 0 ) {
		getInt( 1, ATTR_PROC_ID, "proc" );
	} else {
		job_ad->Assign( ATTR_PROC_ID, job_proc );
	}
	return true;
}


bool
JICLocalConfig::getString( bool warn, const char* attr, 
						   const char* alt_name )
{
	return getAttr( warn, true, attr, alt_name );
}


bool
JICLocalConfig::getBool( bool warn, const char* attr,
						 const char* alt_name )
{
	return getAttr( warn, false, attr, alt_name );
}


bool
JICLocalConfig::getInt( bool warn, const char* attr,
						const char* alt_name )
{
	return getAttr( warn, false, attr, alt_name );
}


bool
JICLocalConfig::getAttr( bool warn, bool is_string, const char* attr, 
						 const char* alt_name )
{
	char* tmp;
	char param_name[256];
	std::string expr;
	bool needs_quotes = false;

	if( job_ad->LookupExpr(attr) ) {
			// we've already got this attribute in our ClassAd, we're
			// done.  
		return true;
	}

	sprintf( param_name, "%s_%s", key, attr );
	tmp = param( param_name );
	if( ! tmp && alt_name ) {
		sprintf( param_name, "%s_%s", key, alt_name );
		tmp = param( param_name );
	}
	if( ! tmp ) {
		if( warn ) {
			dprintf( D_ALWAYS, 
					 "\"%s\" not found in config file\n", 
					 param_name );
		}
		return false;
	}
	if( is_string && tmp[0] != '"' ) {
		needs_quotes = true;
	}

	if( needs_quotes ) {
		expr += "\"";
	}
	expr += tmp;
	if( needs_quotes ) {
		expr += "\"";
	}
	free( tmp );

	if( job_ad->AssignExpr(attr, expr.c_str()) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: Failed to insert into job ad: %s\n",
			 expr.c_str() );
	return false;
}


bool
JICLocalConfig::getUniverse( void ) 
{
	char* tmp;
	char param_name[256];
	int univ = 0;
 
		// first try the ClassAd attr name:
	sprintf( param_name, "%s_%s", key, ATTR_JOB_UNIVERSE );
	tmp = param( param_name );
	if( ! tmp ) {
			// now, try just "key_universe"
		sprintf( param_name, "%s_universe", key );
		tmp = param( param_name );
		if( ! tmp ) {
			dprintf( D_ALWAYS, "\"%s\" not found in config file\n",
					 param_name );
			return false;
		}
	}

		// tmp now holds whatever they told us the universe should be.
		// however, it might be a string universe name, or the integer
		// of the universe we eventually want.  first, see if it's
		// just an integer already...
	univ = atoi( tmp );
	if( ! univ ) {
			// it's not already an int, try to convert from a string. 
		univ = CondorUniverseNumber( tmp );
	}

		// Make sure the universe we job got is valid.  If the user
		// gave a string which wasn't valid, we'll get back a 0, which
		// is the same as CONDOR_UNIVERSE_MIN, so we'll catch it.
	if( univ >= CONDOR_UNIVERSE_MAX || univ <= CONDOR_UNIVERSE_MIN ) { 
		dprintf( D_ALWAYS, 
				 "ERROR: Unrecognized %s \"%s\", aborting\n",
				 param_name, tmp );
		free( tmp );
		return false;
	}
		// we're done with this, so avoid leaking by free'ing now.
	free( tmp );
	tmp = NULL;

	if( ! checkUniverse(univ) ) {
		return false;
	}

	if( job_ad->Assign( ATTR_JOB_UNIVERSE, univ ) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: Failed to insert into job ad: %s = %d\n",
			 ATTR_JOB_UNIVERSE, univ );
	return false;
}



