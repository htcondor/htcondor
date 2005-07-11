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
#include "condor_config.h"
#include "string_list.h"

#include "gt3resource.h"
#include "gridmanager.h"

#define DEFAULT_MAX_PENDING_SUBMITS_PER_RESOURCE	5
#define DEFAULT_MAX_SUBMITTED_JOBS_PER_RESOURCE		100

#define LOG_FILE_TIMEOUT		300

int GT3Resource::gahpCallTimeout = 300;	// default value

#define HASH_TABLE_SIZE			500

HashTable <HashKey, GT3Resource *>
    GT3Resource::ResourcesByName( HASH_TABLE_SIZE,
								  hashFunction );

GT3Resource *GT3Resource::FindOrCreateResource( const char *resource_name,
												const char *proxy_subject )
{
	int rc;
	GT3Resource *resource = NULL;

	const char *canonical_name = CanonicalName( resource_name );
	ASSERT(canonical_name);

	const char *hash_name = HashName( canonical_name, proxy_subject );
	ASSERT(hash_name);

	rc = ResourcesByName.lookup( HashKey( hash_name ), resource );
	if ( rc != 0 ) {
		resource = new GT3Resource( canonical_name, proxy_subject );
		ASSERT(resource);
		if ( resource->Init() == false ) {
			delete resource;
			resource = NULL;
		} else {
			ResourcesByName.insert( HashKey( hash_name ), resource );
		}
	} else {
		ASSERT(resource);
	}

	return resource;
}

GT3Resource::GT3Resource( const char *resource_name,
						  const char *proxy_subject)
	: BaseResource( resource_name )
{
	initialized = false;
	proxySubject = strdup( proxy_subject );
	gahp = NULL;
}

GT3Resource::~GT3Resource()
{
	ResourcesByName.remove( HashKey( resourceName ) );
	daemonCore->Cancel_Timer( pingTimerId );
	if ( gahp != NULL ) {
		delete gahp;
	}
	if ( proxySubject ) {
		free( proxySubject );
	}
}

bool GT3Resource::Init()
{
	if ( initialized ) {
		return true;
	}

	char *gahp_path = param("GT3_GAHP");
	if ( gahp_path == NULL ) {
		dprintf( D_ALWAYS, "GT3_GAHP not defined in condor config file\n" );
		return false;
	} else {
		gahp = new GahpClient( "GT3", gahp_path );
		gahp->setNotificationTimerId( pingTimerId );
		gahp->setMode( GahpClient::normal );
		gahp->setTimeout( gahpCallTimeout );
		free( gahp_path );
	}

	initialized = true;

	Reconfig();

	return true;
}

void GT3Resource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( gahpCallTimeout );
}

const char *GT3Resource::CanonicalName( const char *name )
{
/*
	static MyString canonical;
	char *host;
	char *port;

	parse_resource_manager_string( name, &host, &port, NULL, NULL );

	canonical.sprintf( "%s:%s", host, *port ? port : "2119" );

	free( host );
	free( port );

	return canonical.Value();
*/
	return name;
}

const char *GT3Resource::HashName( const char *resource_name,
								   const char *proxy_subject )
{
	static MyString hash_name;

	hash_name.sprintf( "%s#%s", resource_name, proxy_subject );

	return hash_name.Value();
}

void GT3Resource::DoPing( time_t& ping_delay, bool& ping_complete,
						  bool& ping_succeeded )
{
	int rc;

	if ( gahp->isInitialized() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	gahp->setNormalProxy( gahp->getMasterProxy() );
	if ( PROXY_IS_EXPIRED( gahp->getMasterProxy() ) ) {
		dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
		ping_delay = TIMER_NEVER;
		return;
	}

	ping_delay = 0;

	rc = gahp->gt3_gram_client_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc == GLOBUS_GRAM_PROTOCOL_ERROR_CONTACTING_JOB_MANAGER ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}
