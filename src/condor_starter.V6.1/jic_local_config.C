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
#include "condor_attributes.h"

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
	dprintf( D_ALWAYS, "Getting job ClassAd from config file "
			 "with keyword: \"%s\"\n", key );

	job_ad = new ClassAd();

		// first, things we absolutely need
	if( !getUniverse(job_ad, key) ) { 
		return false;
	}
	if( !getConfigString(job_ad, key, 1, ATTR_JOB_CMD, "executable")) {
		return false;
	}
	if( !getConfigString(job_ad, key, 1, ATTR_JOB_IWD, "initialdir")) {
		return false;
	}

		// ATTR_OWNER only matters if we're running as root, and we'll
		// catch it later if we need it and it's not defined.  so,
		// just treat it as optional at this point.
	getConfigString( job_ad, key, 0, ATTR_OWNER, NULL );

		// now, optional things
	getConfigString( job_ad, key, 0, ATTR_JOB_INPUT, "input" );
	getConfigString( job_ad, key, 0, ATTR_JOB_OUTPUT, "output" );
	getConfigString( job_ad, key, 0, ATTR_JOB_ERROR, "error" );
	getConfigString( job_ad, key, 0, ATTR_JOB_ARGUMENTS, "arguments" );
	getConfigString( job_ad, key, 0, ATTR_JOB_ENVIRONMENT, "environment" );
	getConfigString( job_ad, key, 0, ATTR_JAR_FILES, "jar_files" );
	getConfigInt( job_ad, key, 0, ATTR_KILL_SIG, "kill_sig" );
	getConfigBool( job_ad, key, 0, ATTR_STARTER_WAIT_FOR_DEBUG, 
				   "starter_wait_for_debug" );
	getConfigString( job_ad, key, 0, ATTR_STARTER_ULOG_FILE, "log" );
	getConfigBool( job_ad, key, 0, ATTR_STARTER_ULOG_USE_XML, 
				   "log_use_xml" );

		// only check for cluster and proc in the config file if we
		// didn't get them on the command-line
	MyString line;
	if( job_cluster < 0 ) { 
		getConfigInt( job_ad, key, 0, ATTR_CLUSTER_ID, "cluster" );
	} else {
		line = ATTR_CLUSTER_ID;
		line += '=';
		line += job_cluster;
		job_ad->Insert( line.Value() );
	}
	if( job_proc < 0 ) {
		getConfigInt( job_ad, key, 1, ATTR_PROC_ID, "proc" );
	} else {
		line = ATTR_PROC_ID;
		line += '=';
		line += job_proc;
		job_ad->Insert( line.Value() );
	}
	return true;
}


bool
JICLocalConfig::getConfigString( ClassAd* ad, const char* key, 
								 bool warn, const char* attr,
								 const char* alt_name )
{
	return getConfigAttr( ad, key, warn, true, attr, alt_name );
}


bool
JICLocalConfig::getConfigInt( ClassAd* ad, const char* key, 
							  bool warn, const char* attr,
							  const char* alt_name )
{
	return getConfigAttr( ad, key, warn, false, attr, alt_name );
}


bool
JICLocalConfig::getConfigBool( ClassAd* ad, const char* key, 
							   bool warn, const char* attr,
							   const char* alt_name )
{
	return getConfigAttr( ad, key, warn, false, attr, alt_name );
}


bool
JICLocalConfig::getConfigAttr( ClassAd* ad, const char* key, bool warn, 
							   bool is_string, const char* attr, 
							   const char* alt_name )
{
	char* tmp;
	char param_name[256];
	MyString expr;
	bool needs_quotes = false;

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

	expr = attr;
	expr += " = ";
	if( needs_quotes ) {
		expr += "\"";
	}
	expr += tmp;
	if( needs_quotes ) {
		expr += "\"";
	}
	free( tmp );

	if( ad->Insert(expr.Value()) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: Failed to insert into job ad: %s\n",
			 expr.Value() );
	return false;
}


bool
JICLocalConfig::getUniverse( ClassAd* ad, const char* key ) 
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

		// now that we know what they wanted, see if we'll support it
	switch( univ ) {
	case CONDOR_UNIVERSE_VANILLA:
	case CONDOR_UNIVERSE_JAVA:
			// for now, we don't support much. :)
		break;
	case CONDOR_UNIVERSE_STANDARD:
	case CONDOR_UNIVERSE_PVM:
	case CONDOR_UNIVERSE_SCHEDULER:
	case CONDOR_UNIVERSE_MPI:
	case CONDOR_UNIVERSE_GLOBUS:
	case CONDOR_UNIVERSE_PARALLEL:
			// these are at least valid tries, but we don't work with
			// any of them in stand-alone starter mode... yet.
		dprintf( D_ALWAYS, "ERROR: %s %s (%d) not supported without the "
				 "schedd and/or shadow, aborting\n", param_name,
				 CondorUniverseName(univ), univ );
		return false;
	default:
			// downright unsupported universes
		dprintf( D_ALWAYS, "ERROR: %s %s (%d) is not supported\n", 
				 param_name, CondorUniverseName(univ), univ );
		return false;

	}

		// MyString likes to append strings, so print out the string
		// version of the integer so we can use it.
	char univ_str[32];
	sprintf( univ_str, "%d", univ );

		// finally, we can construct the expression and insert it into
		// the ClassAd
	MyString expr;
	expr = ATTR_JOB_UNIVERSE;
	expr += " = ";
	expr += univ_str;

	if( ad->Insert(expr.Value()) ) {
		return true;
	}
	dprintf( D_ALWAYS, "ERROR: Failed to insert into job ad: %s\n",
			 expr.Value() );
	return false;
}


