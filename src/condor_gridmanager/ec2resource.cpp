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

	std::string error_code;
	int rc = gahp->ec2_vm_server_type( resourceName, m_public_key_file, m_private_key_file, m_serverType, error_code );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} 
	else if ( rc != 0 ) {
		ping_complete = true;
		
		// If the service returns an authorization failure, that means
		// the service is up, so return true.  Individual jobs with
		// invalid authentication tokens will then go on hold, which is
		// what we want (rather than saying idle).
		if( error_code != "" ) {
			if( strstr( error_code.c_str(), "(401)" ) != NULL ) {
				ping_succeeded = true;
				m_hadAuthFailure = true;
				formatstr( authFailureMessage, "(%s): '%s'", error_code.c_str(), gahp->getErrorString() );
			}    		    
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
        std::string errorCode;
        int rc = status_gahp->ec2_vm_status_all( resourceName,
                    m_public_key_file, m_private_key_file,
                    returnStatus, errorCode );

        if( rc == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }
    
        if( rc != 0 ) {
            std::string errorString = status_gahp->getErrorString();
            dprintf( D_ALWAYS, "Error doing batched EC2 status query: %s: %s.\n",
                     errorCode.c_str(), errorString.c_str() );
            return BSR_ERROR;
        }

        //
        // We have to let a job know if we can't find a status report for it.
        //
        List<EC2Job> myJobs;
        EC2Job * nextJob = NULL;
		BaseJob *nextBaseJob = NULL;
		registeredJobs.Rewind();
		while ( (nextBaseJob = registeredJobs.Next()) ) {
			nextJob = dynamic_cast< EC2Job * >( nextBaseJob );
			ASSERT( nextJob );
			if ( !nextJob->m_client_token.empty() ) {
				myJobs.Append( nextJob );
			}
		}

        returnStatus.rewind();
        ASSERT( returnStatus.number() % 6 == 0 );
        for( int i = 0; i < returnStatus.number(); i += 6 ) {
            std::string instanceID = returnStatus.next();
            std::string status = returnStatus.next();
            std::string clientToken = returnStatus.next();
            std::string keyName = returnStatus.next();
            std::string stateReasonCode = returnStatus.next();
            std::string publicDNSName = returnStatus.next();

            // Efficiency suggests we look via the instance ID first,
            // and then try to look things up via the client token
            // (or, for GT #3682, via the keypair ID).

            // We can't use BaseJob::JobsByRemoteId because OpenStack doesn't
            // include the client token in its status responses, and therefore
            // we can't always fully reconstruct the remoteJobID used as the key.
            EC2Job * job = NULL;
            rc = jobsByInstanceID.lookup( HashKey( instanceID.c_str() ), job );
            if( rc == 0 ) {
                ASSERT( job );
        
                dprintf( D_FULLDEBUG, "Found job object for '%s', updating status ('%s').\n", instanceID.c_str(), status.c_str() );
                job->StatusUpdate( instanceID.c_str(), status.c_str(),
                                   stateReasonCode.c_str(), publicDNSName.c_str() );
                myJobs.Delete( job );
                continue;
            }

            // If we got a client token, use that to look up the job.  We
            // don't use the instance ID because we may discover it in
            // this function.  Since we need instance ID -based dispatch
            // code for OpenStack anyway, we'll just use it, rather than
            // trying the remoteJobID with the instance ID if we don't
            // find it using only the client token.
            if( ! clientToken.empty() && clientToken != "NULL" ) {
                std::string remoteJobID;
                formatstr( remoteJobID, "ec2 %s %s", resourceName, clientToken.c_str() );
                
                BaseJob * tmp = NULL;
                rc = BaseJob::JobsByRemoteId.lookup( HashKey( remoteJobID.c_str() ), tmp );
                
                if( rc == 0 ) {
                    ASSERT( tmp );
                    EC2Job * job = dynamic_cast< EC2Job * >( tmp );
                    if( job == NULL ) {
                        EXCEPT( "Found non-EC2Job identified by '%s'.", remoteJobID.c_str() );
                    }
                    
                    dprintf( D_FULLDEBUG, "Found job object via client token for '%s', updating status ('%s').\n", instanceID.c_str(), status.c_str() );
                    job->StatusUpdate( instanceID.c_str(), status.c_str(),
                                       stateReasonCode.c_str(), publicDNSName.c_str() );
                    myJobs.Delete( job );
                    continue;
                }
            }
            
			// Some servers (OpenStack, Eucalyptus) silently ignore client
			// tokens. So we need to use the ssh keypair to find jobs that
			// were submitted but which we don't have an instance ID for.
			//
			// TODO This code should be made more efficient. We can
			//   do something better than a linear scan through all
			//   jobs for each status result. Ideally, we'd parse the
			//   ssh keypair name and if it looks like one we generated,
			//   pluck out the job id.
			if ( !ClientTokenWorks() && !keyName.empty() && keyName != "NULL" ) {
				myJobs.Rewind();
				while ( ( job = myJobs.Next() ) ) {
					if ( job->m_key_pair == keyName ) {
						dprintf( D_FULLDEBUG, "Found job object via ssh keypair for '%s', updating status ('%s').\n", instanceID.c_str(), status.c_str() );
						job->StatusUpdate( instanceID.c_str(), status.c_str(),
										   stateReasonCode.c_str(),
										   publicDNSName.c_str() );
						myJobs.Delete( job );
						continue;
					}
				}
			}

            dprintf( D_FULLDEBUG, "Found unknown instance '%s'; skipping.\n", instanceID.c_str() );
            continue;
        }
    
        myJobs.Rewind();
        while( ( nextJob = myJobs.Next() ) ) {
            dprintf( D_FULLDEBUG, "Informing job %p it got no status.\n", nextJob );
            nextJob->StatusUpdate( NULL, NULL, NULL, NULL );
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
        std::string spotErrorCode;
        int spotRC = status_gahp->ec2_spot_status_all( resourceName,
                        m_public_key_file, m_private_key_file,
                        spotReturnStatus, spotErrorCode );

        if( spotRC == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }

        if( spotRC != 0 ) {
            std::string errorString = status_gahp->getErrorString();
            dprintf( D_ALWAYS, "Error doing batched EC2 spot status query: %s: %s.\n",
                     spotErrorCode.c_str(), errorString.c_str() );
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
            spotJob->StatusUpdate( NULL, state.c_str(), NULL, NULL );
            mySpotJobs.Delete( spotJob );
        }

        mySpotJobs.Rewind();
        while( ( nextSpotJob = mySpotJobs.Next() ) ) {
            dprintf( D_FULLDEBUG, "Informing spot job %p it got no status.\n", nextSpotJob );
            nextSpotJob->StatusUpdate( NULL, NULL, NULL, NULL );
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

bool EC2Resource::ServerTypeQueried( EC2Job *job ) {
	if ( !m_serverType.empty() ) {
		return true;
	}
	std::string type;
	if ( job && job->jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type ) ) {
		return true;
	}
	return false;
}

bool EC2Resource::ClientTokenWorks( EC2Job *job )
{
	if ( m_serverType == "Amazon" || m_serverType == "Nimbus" ) {
		return true;
	}
	std::string type;
	if ( m_serverType.empty() && job &&
		 job->jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type ) ) {
		if ( type == "Amazon" || type == "Nimbus" ) {
			return true;
		}
	}
	return false;
}

bool EC2Resource::ShuttingDownTrusted( EC2Job *job ) {
	std::string type = m_serverType;
	if( type.empty() && job ) {
		// CODE REVIEWER: This assumes that LookupString() will leave
		// type alone when it fails.
		job->jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type );
	}

	if( type == "Amazon" ) { return true; }
	return false;
}

