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

#include "nordugridresource.h"
#include "gridmanager.h"

template class List<NordugridJob>;
template class Item<NordugridJob>;
template class HashTable<HashKey, NordugridResource *>;
template class HashBucket<HashKey, NordugridResource *>;

#define HASH_TABLE_SIZE			500

HashTable <HashKey, NordugridResource *>
    NordugridResource::ResourcesByName( HASH_TABLE_SIZE,
										hashFunction );

NordugridResource *NordugridResource::FindOrCreateResource( const char * resource_name )
{
	int rc;
	NordugridResource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( resource_name ), resource );
	if ( rc != 0 ) {
		resource = new NordugridResource( resource_name );
		ASSERT(resource);
		ResourcesByName.insert( HashKey( resource_name ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}

NordugridResource::NordugridResource( const char *resource_name )
	: BaseResource( resource_name )
{
	connectionUser = NULL;
	ftpServer = NULL;

	registeredJobs = new List<NordugridJob>;
	connectionWaiters = new List<NordugridJob>;
}

NordugridResource::~NordugridResource()
{
	CloseConnection();
	if ( registeredJobs != NULL ) {
		delete registeredJobs;
	}
	if ( connectionWaiters != NULL ) {
		delete connectionWaiters;
	}
}

bool NordugridResource::IsEmpty()
{
	return registeredJobs->IsEmpty();
}

void NordugridResource::Reconfig()
{
	BaseResource::Reconfig();
}

void NordugridResource::RegisterJob( NordugridJob *job )
{
	registeredJobs->Append( job );
}

void NordugridResource::UnregisterJob( NordugridJob *job )
{
	registeredJobs->Delete( job );

		// TODO: if this is last job, arrange to close session and delete
		//   this object
}

int NordugridResource::AcquireConnection( NordugridJob *job,
										  ftp_lite_server *&server )
{
	NordugridJob *jobptr;

	if ( connectionUser == NULL ) {
		connectionUser = job;
	} else if ( connectionUser != job ) {
		connectionWaiters->Rewind();
		while ( connectionWaiters->Next( jobptr ) ) {
			if ( jobptr == job ) {
				return ACQUIRE_QUEUED;
			}
		}
		connectionWaiters->Append( job );
		return ACQUIRE_QUEUED;
	}

	if ( OpenConnection() ) {
		server = ftpServer;
		return ACQUIRE_DONE;
	} else {
		return ACQUIRE_FAILED;
	}
}

void NordugridResource::ReleaseConnection( NordugridJob *job )
{
	if ( connectionUser == NULL ) {
		return;
	} else if ( connectionUser != job ) {
		connectionWaiters->Delete( job );
		return;
	}

	connectionUser = NULL;

	if ( connectionWaiters->IsEmpty() == false ) {
		connectionWaiters->Rewind();
		connectionUser = connectionWaiters->Next();
		connectionWaiters->DeleteCurrent();
		connectionUser->SetEvaluateState();
	}

		// set timer to close connection after a time?
}

bool NordugridResource::OpenConnection()
{
	if ( ftpServer != NULL ) {
		return true;
	}

	ftpServer = ftp_lite_open( resourceName, 2811, NULL );
	if ( ftpServer == NULL ) {
		dprintf( D_FULLDEBUG, "ftp_lite_open failed!\n" );
		return false;
	}

	if ( ftp_lite_auth_globus( ftpServer ) == 0 ) {
		dprintf( D_FULLDEBUG, "ftp_lite_auth_globus() failed\n" );
		ftp_lite_close( ftpServer );
		ftpServer = NULL;
		return false;
	}

	return true;
}

bool NordugridResource::CloseConnection()
{
	if ( ftpServer == NULL ) {
		return true;
	}

	ftp_lite_close( ftpServer );
	ftpServer = NULL;

	return true;
}
