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

#include "ec2resource.h"
#include "gridmanager.h"

std::map <std::string, EC2Resource *>
    EC2Resource::ResourcesByName;

std::string & EC2Resource::HashName( const char * resource_name,
		const char * public_key_file, const char * private_key_file )
{
	static std::string hash_name;
	formatstr( hash_name, "ec2 %s#%s#%s", resource_name, public_key_file, private_key_file );
	return hash_name;
}


EC2Resource* EC2Resource::FindOrCreateResource(const char * resource_name,
	const char * public_key_file, const char * private_key_file )
{
	EC2Resource *resource = NULL;
	std::string &key = HashName( resource_name, public_key_file, private_key_file );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new EC2Resource( resource_name, public_key_file, private_key_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName[key] = resource;
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}


EC2Resource::EC2Resource( const char *resource_name,
	const char * public_key_file, const char * private_key_file ) :
		BaseResource( resource_name ),
		m_hadAuthFailure( false ),
		m_checkSpotNext( false )
{
	// although no one will use resource_name, we still keep it for base class constructor

	m_public_key_file = strdup(public_key_file);
	m_private_key_file = strdup(private_key_file);

	status_gahp = gahp = NULL;

	char * gahp_path = param( "EC2_GAHP" );
	if ( gahp_path == NULL ) {
		dprintf(D_ALWAYS, "EC2_GAHP not defined! \n");
		return;
	}

	ArgList args;
	args.AppendArg("-f");

	// Set up & zero the statistics pool before we start the GAHP.
	gs.Init();
	gs.Clear();

	std::string gahp_name;
	formatstr( gahp_name, "EC2-%s@%s", m_public_key_file, resource_name );
	// dprintf( D_ALWAYS, "Using %s for GAHP name.\n", gahp_name.c_str() );
	gahp = new EC2GahpClient( gahp_name.c_str(), gahp_path, &args );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( EC2Job::gahpCallTimeout );

	status_gahp = new EC2GahpClient( gahp_name.c_str(), gahp_path, &args );

	StartBatchStatusTimer();

    status_gahp->setNotificationTimerId( BatchPollTid() );
	status_gahp->setMode( GahpClient::normal );
	status_gahp->setTimeout( EC2Job::gahpCallTimeout );

	free(gahp_path);
}


EC2Resource::~EC2Resource()
{
	ResourcesByName.erase( HashName( resourceName, m_public_key_file,
										m_private_key_file ) );
	if ( gahp ) delete gahp;
	if (m_public_key_file) free(m_public_key_file);
	if (m_private_key_file) free(m_private_key_file);
}


void EC2Resource::Reconfig()
{
	BaseResource::Reconfig();
	gahp->setTimeout( EC2Job::gahpCallTimeout );
	status_gahp->setTimeout( EC2Job::gahpCallTimeout );
}

const char *EC2Resource::ResourceType()
{
	return "ec2";
}

const char *EC2Resource::GetHashName()
{
	return HashName( resourceName, m_public_key_file, m_private_key_file ).c_str();
}

void EC2Resource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	resource_ad->Assign( ATTR_EC2_ACCESS_KEY_ID, m_public_key_file );
	resource_ad->Assign( ATTR_EC2_SECRET_ACCESS_KEY, m_private_key_file );

	gahp->PublishStats( resource_ad );

	// Also publish the statistics we got from the GAHP.
	gs.Publish( * resource_ad );
}

// we will use ec2 command "status_all" to do the Ping work
void EC2Resource::DoPing(time_t& ping_delay, bool& ping_complete, bool& ping_succeeded )
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

    // First, fetch the GAHP's statistics.
    std::vector<std::string> statistics;
    int rc = status_gahp->ec2_gahp_statistics( statistics );

    if( rc == GAHPCLIENT_COMMAND_PENDING ) { return BSR_PENDING; }
    if( rc != 0 ) {
        std::string errorString = status_gahp->getErrorString();
        dprintf( D_ALWAYS, "Failed to get GAHP statistics: %s.\n",
                 errorString.c_str() );
        return BSR_ERROR;
    }

    // Roll the ring buffer once to make room for the new statistics.
    gs.Advance();

    // Update the statistics with the numbers from the GAHP.
    ASSERT( statistics.size() == 4 );
    gs.NumRequests = atoi( statistics[0].c_str() );
    gs.NumDistinctRequests = atoi( statistics[1].c_str() );
    gs.NumRequestsExceedingLimit = atoi( statistics[2].c_str() );
    gs.NumExpiredSignatures = atoi( statistics[3].c_str() );

    // m_checkSpotNext starts out false
    if( ! m_checkSpotNext ) {
        std::vector<std::string> returnStatus;
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
		std::vector<EC2Job*> myJobs;
		EC2Job * nextJob = nullptr;
		for (auto nextBaseJob: registeredJobs) {
			nextJob = dynamic_cast< EC2Job * >( nextBaseJob );
			ASSERT( nextJob );
			if ( !nextJob->m_client_token.empty() ) {
				myJobs.push_back(nextJob);
			}
		}

        ASSERT( returnStatus.size() % 8 == 0 );
        for( size_t i = 0; i < returnStatus.size(); i += 8 ) {
            std::string instanceID = returnStatus[i];
            std::string status = returnStatus[i + 1];
            std::string clientToken = returnStatus[i + 2];
            std::string keyName = returnStatus[i + 3];
            std::string stateReasonCode = returnStatus[i + 4];
            std::string publicDNSName = returnStatus[i + 5];
            /* std::string spotFleetRequestID = returnStatus[i + 6]; */
            /* std::string annexName = returnStatus[i + 7]; */

            // Efficiency suggests we look via the instance ID first,
            // and then try to look things up via the client token
            // (or, for GT #3682, via the keypair ID).

            // We can't use BaseJob::JobsByRemoteId because OpenStack doesn't
            // include the client token in its status responses, and therefore
            // we can't always fully reconstruct the remoteJobID used as the key.
            EC2Job * job = NULL;
            auto it = jobsByInstanceID.find(instanceID);
            if (it != jobsByInstanceID.end()) {
                job = it->second;
                ASSERT( job );

                dprintf( D_FULLDEBUG, "(%d.%d) Found job object for '%s', updating status ('%s') and state reason code ('%s').\n", job->procID.cluster, job->procID.proc, instanceID.c_str(), status.c_str(), stateReasonCode.c_str() );
                job->StatusUpdate( instanceID.c_str(), status.c_str(),
                                   stateReasonCode.c_str(), publicDNSName.c_str(), NULL );
                std::erase(myJobs, job);
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

                auto itr = BaseJob::JobsByRemoteId.find(remoteJobID);

                if( itr != BaseJob::JobsByRemoteId.end() ) {
                    ASSERT( itr->second );
                    EC2Job * job = dynamic_cast< EC2Job * >( itr->second );
                    if( job == NULL ) {
                        EXCEPT( "Found non-EC2Job identified by '%s'.", remoteJobID.c_str() );
                    }

                    dprintf( D_FULLDEBUG, "(%d.%d) Found job object for '%s' using client token '%s', updating status ('%s') and state reason code ('%s').\n", job->procID.cluster, job->procID.proc, instanceID.c_str(), clientToken.c_str(), status.c_str(), stateReasonCode.c_str() );
                    job->StatusUpdate( instanceID.c_str(), status.c_str(),
                                       stateReasonCode.c_str(), publicDNSName.c_str(), NULL );
                    std::erase(myJobs, job);
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
				auto it = myJobs.begin();
				while (it != myJobs.end()) {
					EC2Job* job = *it;
					if ( job->m_key_pair == keyName ) {
						dprintf( D_FULLDEBUG, "(%d.%d) Found job object via ssh keypair for '%s', updating status ('%s') and state reason code ('%s').\n", job->procID.cluster, job->procID.proc, instanceID.c_str(), status.c_str(), stateReasonCode.c_str() );
						job->StatusUpdate( instanceID.c_str(), status.c_str(),
										   stateReasonCode.c_str(),
										   publicDNSName.c_str(), NULL );
						it = myJobs.erase(it);
					} else {
						it++;
					}
				}
			}

            dprintf( D_FULLDEBUG, "Found unknown instance '%s'; skipping.\n", instanceID.c_str() );
            continue;
        }

		// We need to check for spot prices in the absence of spot request
		// IDs in order to handle recovery efficiently.  It would be cleaner
		// not to call StatusUpdate() on jobs which were in that state, but
		// since they're hard to distinguish from spot /instances/ that have
		// vanished, and I think we already handle the fake-purge case OK
		// (possibly because of updating all jobs, not just non-spot jobs
		// earlier), so let's just do it.
		bool hadSpotPrice = false;
		for (auto nextJob: myJobs) {
			if(! nextJob->m_spot_price.empty()) {
				hadSpotPrice = true;
			}

			auto spot_it = spotJobsByRequestID.end();
			std::string requestID = nextJob->m_spot_request_id;
			if(! requestID.empty()) {
				spot_it = spotJobsByRequestID.find(requestID);
			}

			if( spot_it != spotJobsByRequestID.end() ) {
				dprintf( D_FULLDEBUG, "(%d.%d) Not informing job it got no status because it's a registered spot request.\n", nextJob->procID.cluster, nextJob->procID.proc );
			} else {
				dprintf( D_FULLDEBUG, "(%d.%d) Informing job it got no status.\n", nextJob->procID.cluster, nextJob->procID.proc );
				nextJob->StatusUpdate( NULL, NULL, NULL, NULL, NULL );
			}
		}

        // Don't ask for spot results unless we know about a spot job.  This
        // should prevent us from breaking OpenStack.
        if( spotJobsByRequestID.size() == 0 && (! hadSpotPrice) ) {
            m_checkSpotNext = false;
            return BSR_DONE;
        } else {
            m_checkSpotNext = true;
        }
    }

    if( m_checkSpotNext ) {
        std::vector<std::string> spotReturnStatus;
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

		std::vector<EC2Job*> mySpotJobs;
		for (const auto& it: spotJobsByRequestID) {
			mySpotJobs.push_back(it.second);
		}

        ASSERT( spotReturnStatus.size() % 5 == 0 );
        for( size_t i = 0; i < spotReturnStatus.size(); i += 5 ) {
            std::string requestID = spotReturnStatus[i];
            std::string state = spotReturnStatus[i + 1];
            std::string launchGroup = spotReturnStatus[i + 2];
            std::string instanceID = spotReturnStatus[i + 3];
            std::string statusCode = spotReturnStatus[i + 4];

			EC2Job * spotJob = NULL;
			auto spot_it = spotJobsByRequestID.find(requestID);
			if( spot_it != spotJobsByRequestID.end() ) {
				spotJob = spot_it->second;
			} else {
				// dprintf( D_FULLDEBUG, "Failed to find spot request '%s' by ID, looking for client token '%s'...\n", requestID.c_str(), launchGroup.c_str() );

				// The SIR's "launch group" is its client token.  This lets
				// us recover jobs which crashed between requesting a spot
				// instance and recording the request ID.
				EC2Job * nextJob = NULL;
				for (auto nextBaseJob: registeredJobs) {
					nextJob = dynamic_cast< EC2Job * >( nextBaseJob );
					ASSERT( nextJob );

					// dprintf( D_FULLDEBUG, "... does it match '%s'?\n", nextJob->m_client_token.c_str() );
					if( nextJob->m_client_token == launchGroup ) {
						break;
					}
				}

    	        //
	            // PROBLEM -- GM_SPOT_CANCEL deliberately removes spot jobs
        	    // from the list of spot instances because they've become
            	// regular instance jobs.  However, they still have their
            	// client tokens...
            	//
            	// FIXME: This will probably work, but the real solution
            	// is register both spot /and/ dedicated jobs in their own
            	// hashtables, and only use the table of all registered jobs
            	// to assert that each registered job is either a spot job
            	// or an instance job.
            	//
				if( nextJob && nextJob->m_client_token == launchGroup ) {
					if( nextJob->m_remoteJobId.empty() ) {
						dprintf( D_FULLDEBUG, "(%d.%d) Found spot job object for '%s' using client token '%s'; updating status ('%s') and state reason code ('%s').\n", nextJob->procID.cluster, nextJob->procID.proc, requestID.c_str(), launchGroup.c_str(), state.c_str(), statusCode.c_str() );
						nextJob->StatusUpdate( instanceID.c_str(), state.c_str(), statusCode.c_str(), NULL, requestID.c_str() );
					} else {
						dprintf( D_FULLDEBUG, "(%d.%d) Not updating spot job object for '%s' because it has an instance ID already.\n", nextJob->procID.cluster, nextJob->procID.proc, requestID.c_str() );
					}
					// We don't need to remove this job from mySpotJobs
					// because it can't have been present in it.
				} else {
					dprintf( D_FULLDEBUG, "Found unknown spot request '%s'; skipping.\n", requestID.c_str() );
				}
                continue;
            }
            ASSERT( spotJob );

            dprintf( D_FULLDEBUG, "Found spot job object for '%s', updating status ('%s') and state reason code ('%s').\n", requestID.c_str(), state.c_str(), statusCode.c_str() );
            spotJob->StatusUpdate( instanceID.c_str(), state.c_str(), statusCode.c_str(), NULL, NULL );
            std::erase(mySpotJobs, spotJob);
        }

        for (auto nextSpotJob: mySpotJobs) {
            dprintf( D_FULLDEBUG, "Informing spot job %p it got no status.\n", nextSpotJob );
            nextSpotJob->StatusUpdate( NULL, NULL, NULL, NULL, NULL );
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

bool EC2Resource::ServerTypeQueried( EC2Job *job ) const {
	if ( !m_serverType.empty() ) {
		return true;
	}
	std::string type;
	if ( job && job->jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type ) ) {
		return true;
	}
	return false;
}

bool EC2Resource::ClientTokenWorks( EC2Job *job ) const
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

bool EC2Resource::ShuttingDownTrusted( EC2Job *job ) const {
	std::string type = m_serverType;
	if( type.empty() && job ) {
		// CODE REVIEWER: This assumes that LookupString() will leave
		// type alone when it fails.
		job->jobAd->LookupString( ATTR_EC2_SERVER_TYPE, type );
	}

	if( type == "Amazon" ) { return true; }
	return false;
}

#define SAMPLES 4
#define QUANTUM 1
EC2Resource::GahpStatistics::GahpStatistics() :
	NumRequests(SAMPLES), NumDistinctRequests(SAMPLES),
	NumRequestsExceedingLimit(SAMPLES), NumExpiredSignatures(SAMPLES) { }

#define ADD_PROBE(name) AddProbe( #name, & name )
void EC2Resource::GahpStatistics::Init() {

	pool.ADD_PROBE( NumRequests );
	pool.ADD_PROBE( NumDistinctRequests );
	pool.ADD_PROBE( NumRequestsExceedingLimit );
	pool.ADD_PROBE( NumExpiredSignatures );

	pool.SetRecentMax( SAMPLES, QUANTUM );
}

void EC2Resource::GahpStatistics::Clear() {
	pool.Clear();

    NumRequests = 0;
    NumDistinctRequests = 0;
    NumRequestsExceedingLimit = 0;
    NumExpiredSignatures = 0;
}

void EC2Resource::GahpStatistics::Advance() {
	pool.Advance( QUANTUM );
}

void EC2Resource::GahpStatistics::Publish( ClassAd & ad ) const {
	pool.Publish( ad, (IF_BASICPUB & IF_PUBLEVEL) | IF_RECENTPUB );
}

void EC2Resource::GahpStatistics::Unpublish( ClassAd & ad ) const {
	pool.Unpublish( ad );
}
