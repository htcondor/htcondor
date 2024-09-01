/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

#include "arcresource.h"
#include "arcjob.h"
#include "gridmanager.h"

#define HTTP_200_OK			200
#define HTTP_201_CREATED	201
#define HTTP_202_ACCEPTED	202

std::map <std::string, ArcResource *>
    ArcResource::ResourcesByName;

std::string& ArcResource::HashName( const char *resource_name,
                                    int gahp_id,
                                    const char *proxy_subject,
                                    const std::string& token_file )
{
	static std::string hash_name;

	formatstr( hash_name, "arc %s#%d#%s#%s", resource_name,
	           gahp_id,
	           proxy_subject ? proxy_subject : "NULL",
	           token_file.c_str() );

	return hash_name;
}

ArcResource *ArcResource::FindOrCreateResource( const char * resource_name,
                                                int gahp_id,
                                                const Proxy *proxy,
                                                const std::string& token_file )
{
	ArcResource *resource = NULL;
	const std::string &key = HashName( resource_name, gahp_id, proxy ? proxy->subject->fqan : "", token_file );
	auto itr = ResourcesByName.find( key );
	if ( itr == ResourcesByName.end() ) {
		resource = new ArcResource( resource_name, gahp_id, proxy, token_file );
		ASSERT(resource);
		resource->Reconfig();
		ResourcesByName[key] = resource;
	} else {
		resource = itr->second;
		ASSERT(resource);
	}

	return resource;
}

ArcResource::ArcResource( const char *resource_name,
                          int gahp_id,
                          const Proxy *proxy,
                          const std::string& token_file )
	: BaseResource( resource_name )
{
	proxySubject = proxy ? strdup( proxy->subject->subject_name ) : nullptr;
	proxyFQAN = proxy ? strdup( proxy->subject->fqan ) : nullptr;
	m_tokenFile = token_file;
	m_gahpId = gahp_id;

	gahp = NULL;

	std::string buff;
	formatstr( buff, "ARC/%d/%s#%s",
	           m_gahpId,
	           (m_tokenFile.empty() && proxyFQAN) ? proxyFQAN : "",
	           m_tokenFile.c_str() );

	gahp = new GahpClient( buff.c_str() );
	gahp->setNotificationTimerId( pingTimerId );
	gahp->setMode( GahpClient::normal );
	gahp->setTimeout( ArcJob::gahpCallTimeout );

	m_jobStatusTid = TIMER_UNSET;
	m_jobStatusTid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&ArcResource::DoJobStatus,
							"ArcResource::DoJobStatus", (Service*)this );

	m_statusGahp = new GahpClient( buff.c_str() );
	m_statusGahp->setNotificationTimerId( m_jobStatusTid );
	m_statusGahp->setMode( GahpClient::normal );
	m_statusGahp->setTimeout( ArcJob::gahpCallTimeout );

	m_jobStatusActive = false;
}

ArcResource::~ArcResource()
{
	ResourcesByName.erase( HashName( resourceName, m_gahpId, proxyFQAN, m_tokenFile ) );
	free( proxyFQAN );
	if ( proxySubject ) {
		free( proxySubject );
	}
	if ( gahp ) {
		delete gahp;
	}
	delete m_statusGahp;
	if ( m_jobStatusTid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( m_jobStatusTid );
	}
}

void ArcResource::Reconfig()
{
	BaseResource::Reconfig();

	gahp->setTimeout( ArcJob::gahpCallTimeout );
}

const char *ArcResource::ResourceType()
{
	return "arc";
}

const char *ArcResource::GetHashName()
{
	return HashName( resourceName, m_gahpId, proxyFQAN, m_tokenFile ).c_str();
}

void ArcResource::PublishResourceAd( ClassAd *resource_ad )
{
	BaseResource::PublishResourceAd( resource_ad );

	if (proxySubject) {
		resource_ad->Assign( ATTR_X509_USER_PROXY_SUBJECT, proxySubject );
	}
	if (proxyFQAN) {
		resource_ad->Assign( ATTR_X509_USER_PROXY_FQAN, proxyFQAN );
	}
	if ( !m_tokenFile.empty() ) {
		resource_ad->Assign( ATTR_SCITOKENS_FILE, m_tokenFile );
	}

	gahp->PublishStats( resource_ad );
}

void ArcResource::DoPing( unsigned& ping_delay, bool& ping_complete,
							bool& ping_succeeded )
{
	int rc;

	if ( gahp->isStarted() == false ) {
		dprintf( D_ALWAYS,"gahp server not up yet, delaying ping\n" );
		ping_delay = 5;
		return;
	}
	if (proxySubject) {
		gahp->setNormalProxy( gahp->getMasterProxy() );
		if ( PROXY_IS_EXPIRED( gahp->getMasterProxy() ) ) {
			dprintf( D_ALWAYS,"proxy near expiration or invalid, delaying ping\n" );
			ping_delay = TIMER_NEVER;
			return;
		}
	}

	ping_delay = 0;

	rc = gahp->arc_ping( resourceName );

	if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
		ping_complete = false;
	} else if ( rc != HTTP_200_OK ) {
		ping_complete = true;
		ping_succeeded = false;
	} else {
		ping_complete = true;
		ping_succeeded = true;
	}
}

void ArcResource::DoJobStatus( int /* timerID */ )
{
	if ( ( registeredJobs.empty() || resourceDown ) &&
		 m_jobStatusActive == false ) {
			// No jobs or we can't talk to the resource, so no point
			// in polling
		daemonCore->Reset_Timer( m_jobStatusTid, m_paramJobPollInterval );
		return;
	}

	if ( m_statusGahp->isStarted() == false ) {
		// The gahp isn't started yet. Wait a few seconds for a Job
		// object to start it and initialize x509 credentials.
		daemonCore->Reset_Timer( m_jobStatusTid, 5 );
		return;
	}

	daemonCore->Reset_Timer( m_jobStatusTid, TIMER_NEVER );

	std::vector<std::string> job_ids;
	std::vector<std::string> job_states;

	if ( m_jobStatusActive == false ) {

			// start group status command
		dprintf( D_FULLDEBUG, "Starting ARC group status query: %s\n", resourceName );

		int rc = m_statusGahp->arc_job_status_all( resourceName, "",
		                                           job_ids, job_states );
		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			dprintf( D_ALWAYS,
					 "gahp->arc_job_status_all returned %d for resource %s\n",
					 rc, resourceName );
			EXCEPT( "arc_job_status_all failed!" );
		}
		m_jobStatusActive = true;

	} else {

			// finish status command
		int rc = m_statusGahp->arc_job_status_all( resourceName, "", job_ids, job_states );

		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
			return;
		} else if ( rc != HTTP_201_CREATED ) {
			dprintf( D_ALWAYS,
					 "gahp->arc_job_status_all returned %d for resource %s: %s\n",
					 rc, resourceName, m_statusGahp->getErrorString() );
			dprintf( D_ALWAYS, "Requesting ping of resource\n" );
			RequestPing( NULL );
		}

		if ( rc == HTTP_201_CREATED ) {
			std::string key;

			ASSERT(job_ids.size() == job_states.size());
			for (size_t i = 0; i < job_ids.size(); i++) {
				ArcJob *job = NULL;
				formatstr(key, "arc %s %s", resourceName, job_ids[i].c_str());
				auto itr = BaseJob::JobsByRemoteId.find(key);
				if ( itr != BaseJob::JobsByRemoteId.end() && (job = dynamic_cast<ArcJob*>(itr->second)) ) {
					job->NotifyNewRemoteStatus(job_states[i].c_str());
				}
			}

		}

		m_jobStatusActive = false;

		dprintf( D_FULLDEBUG, "group status query complete: %s\n", resourceName );

		daemonCore->Reset_Timer( m_jobStatusTid, m_paramJobPollInterval );
	}
}
