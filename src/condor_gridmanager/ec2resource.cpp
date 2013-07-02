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
#include "string_list.h"

#include "ec2resource.h"
#include "gridmanager.h"

#define HASH_TABLE_SIZE	500

HashTable <HashKey, EC2Resource *> 
	EC2Resource::ResourcesByName( HASH_TABLE_SIZE, hashFunction );

const char * EC2Resource::HashName( const char * resource_name,
		const char * public_key_file, const char * private_key_file )
{								 
	static std::string hash_name;
	formatstr( hash_name, "ec2 %s#%s#%s", resource_name, public_key_file, private_key_file );
	return hash_name.c_str();
}


EC2Resource* EC2Resource::FindOrCreateResource(const char * resource_name, 
	const char * public_key_file, const char * private_key_file )
{
	int rc;
	EC2Resource *resource = NULL;

	rc = ResourcesByName.lookup( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	if ( rc != 0 ) {
		resource = new EC2Resource( resource_name, public_key_file, private_key_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName.insert( HashKey( HashName( resource_name, public_key_file, private_key_file ) ), resource );
	} else {
		ASSERT(resource);
	}

	return resource;
}


EC2Resource::EC2Resource( const char *resource_name, 
	const char * public_key_file, const char * private_key_file ) :
		BaseResource( resource_name ),
		jobsByInstanceID( hashFunction ),
		spotJobsByRequestID( hashFunction ),
		m_hadAuthFailure( false ),
		m_checkSpotNext( false )
{
	// although no one will use resource_name, we still keep it for base class constructor
	
	m_public_key_file = strdup(public_key_file);
	m_private_key_file = strdup(private_key_file);
	
	gahp = NULL;

	char * gahp_path = param( "EC2_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "EC2_GAHP not defined! \n");
		return;
	}
	
	ArgList args;
	args.AppendArg("-f");

	gahp = new GahpClient( EC2_RESOURCE_NAME, gahp_path, &args );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( EC2Job::gahpCallTimeout );

	status_gahp = new GahpClient( EC2_RESOURCE_NAME, gahp_path, &args );

	StartBatchStatusTimer();

    status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( EC2Job::gahpCallTimeout );

	free(gahp_path);
}


EC2Resource::~EC2Resource()
{
	ResourcesByName.remove( HashKey( HashName( resourceName, m_public_key_file,
											   m_private_key_file ) ) );
	if ( gahp ) delete gahp;
	if (m_public_key_file) free(m_public_key_file);
	if (m_private_key_file) free(m_private_key_file);
}


void EC2Resource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( EC2Job::gahpCallTimeout );
}

const char *EC2Resource::ResourceType()
{
	return "ec2";
}

const char *EC2Resource::GetHashName()
{
	return HashName( resourceName, m_public_key_file, m_private_key_file );
}

void EC2Resource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_EC2_ACCESS_KEY_ID, m_public_key_file );
	resource_ad->Assign( ATTR_EC2_SECRET_ACCESS_KEY, m_private_key_file );
}

// we will use ec2 command "status_all" to do the Ping work
void EC2Resource::DoPing( time_t& ping_delay, bool& ping_complete, bool& ping_succeeded )
{
	// Since EC2 doesn't use proxy, we should use Startup() to replace isInitialized()
	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	
	ping_delay = 0;

	char * error_code = NULL;	
	int rc = gahp->ec2_ping( resourceName, m_public_key_file, m_private_key_file, error_code );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		
		// If the service returns an authorization failure, that means
		// the service is up, so return true.  Individual jobs with
		// invalid authentication tokens will then go on hold, which is
		// what we want (rather than saying idle).
		if( error_code != NULL ) {
			if( strstr( error_code, "(401)" ) != NULL ) {
				ping_succeeded = true;
				m_hadAuthFailure = true;
				formatstr( authFailureMessage, "(%s): '%s'", error_code, gahp->getErrorString() );
			}    		    
			free( error_code );
		} else {
		    ping_succeeded = false;
		}
	} 
	else {
		ping_complete = true;
		ping_succeeded = true;
	}

	return;
}

EC2Resource::BatchStatusResult EC2Resource::StartBatchStatus() {
    ASSERT( status_gahp );

    // m_checkSpotNext starts out false
    if( ! m_checkSpotNext ) {
        StringList returnStatus;
        char * errorCode = NULL;
        int rc = status_gahp->ec2_vm_status_all( resourceName,
                    m_public_key_file, m_private_key_file,
                    returnStatus, errorCode );

        if( rc == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }
    
        if( rc != 0 ) {
            std::string errorString = status_gahp->getErrorString();
            dprintf( D_ALWAYS, "Error doing batched EC2 status query: %s: %s.\n",
                     errorCode ? errorCode : "", errorString.c_str() );
            free( errorCode );
            return BSR_ERROR;
        }

        //
        // We have to let a job know if we can't find a status report for it.
        //
        List<EC2Job> myJobs;
        EC2Job * nextJob = NULL;
        jobsByInstanceID.startIterations();
        while( jobsByInstanceID.iterate( nextJob ) ) {
            myJobs.Append( nextJob );
        }

        returnStatus.rewind();
        ASSERT( returnStatus.number() % 4 == 0 );
        for( int i = 0; i < returnStatus.number(); i += 4 ) {
            std::string instanceID = returnStatus.next();
            std::string status = returnStatus.next();
            /* std::string amiID = */ returnStatus.next();
            /* std::string clientToken = */ returnStatus.next();
       
            // We can't use BaseJob::JobsByRemoteId because OpenStack doesn't
            // include the client token in its status responses, and therefore
            // we can't always fully reconstruct the remoteJobID used as the key.
            EC2Job * job = NULL;
            rc = jobsByInstanceID.lookup( HashKey( instanceID.c_str() ), job );
            if( rc != 0 ) {
                dprintf( D_FULLDEBUG, "Found unknown instance '%s'; skipping.\n", instanceID.c_str() );
                continue;
            }
            ASSERT( job );
        
            dprintf( D_FULLDEBUG, "Found job object for '%s', updating status ('%s').\n", instanceID.c_str(), status.c_str() );
            job->StatusUpdate( status.c_str() );
            myJobs.Delete( job );
        }
    
        myJobs.Rewind();
        while( ( nextJob = myJobs.Next() ) ) {
            dprintf( D_FULLDEBUG, "Informing job %p it got no status.\n", nextJob );
            nextJob->StatusUpdate( NULL );
        }
    
        // Don't ask for spot results unless we know about a spot job.  This
        // should prevent us from breaking OpenStack.
        if( spotJobsByRequestID.getNumElements() == 0 ) {
            m_checkSpotNext = false;
            return BSR_DONE;
        } else {
            m_checkSpotNext = true;
        }
    }
    
    if( m_checkSpotNext ) {
        StringList spotReturnStatus;
        char * spotErrorCode = NULL;
        int spotRC = status_gahp->ec2_spot_status_all( resourceName,
                        m_public_key_file, m_private_key_file,
                        spotReturnStatus, spotErrorCode );

        if( spotRC == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }

        if( spotRC != 0 ) {
            std::string errorString = status_gahp->getErrorString();
            dprintf( D_ALWAYS, "Error doing batched EC2 spot status query: %s: %s.\n",
                     spotErrorCode ? spotErrorCode : "", errorString.c_str() );
            free( spotErrorCode );
            return BSR_ERROR;
        }

        List<EC2Job> mySpotJobs;
        EC2Job * nextSpotJob = NULL;
        spotJobsByRequestID.startIterations();
        while( spotJobsByRequestID.iterate( nextSpotJob ) ) {
            mySpotJobs.Append( nextSpotJob );
        }
    
        spotReturnStatus.rewind();
        ASSERT( spotReturnStatus.number() % 5 == 0 );
        for( int i = 0; i < spotReturnStatus.number(); i += 5 ) {
            // Note that we called 
            std::string requestID = spotReturnStatus.next();
            std::string state = spotReturnStatus.next();
            /* std::string launchGroup = */ spotReturnStatus.next();
            /* std::string instanceID = */ spotReturnStatus.next();
            std::string statusCode = spotReturnStatus.next();
            
            EC2Job * spotJob = NULL;
            spotRC = spotJobsByRequestID.lookup( HashKey( requestID.c_str() ), spotJob );
            if( spotRC != 0 ) {
                dprintf( D_FULLDEBUG, "Found unknown spot request '%s'; skipping.\n", requestID.c_str() );
                continue;
            }
            ASSERT( spotJob );

            if( ! statusCode.empty() ) { state = statusCode; }

            dprintf( D_FULLDEBUG, "Found spot job object for '%s', updating status ('%s').\n", requestID.c_str(), state.c_str() );
            spotJob->StatusUpdate( state.c_str() );
            mySpotJobs.Delete( spotJob );
        }

        mySpotJobs.Rewind();
        while( ( nextSpotJob = mySpotJobs.Next() ) ) {
            dprintf( D_FULLDEBUG, "Informing spot job %p it got no status.\n", nextSpotJob );
            nextSpotJob->StatusUpdate( NULL );
        }
        
        m_checkSpotNext = false;
        return BSR_DONE;
    }

    // This should never happen (but the compiler hates you).
    return BSR_ERROR;
}

EC2Resource::BatchStatusResult EC2Resource::FinishBatchStatus() {
    return StartBatchStatus();
}
