/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "condor_io.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_commands.h"
#include "matchmaker_negotiate.h"

ResourceRequestList::ResourceRequestList(int protocol_version)
	: m_send_end_negotiate(false),
	m_send_end_negotiate_now(false),
	m_requests_to_fetch(0)
{
	m_protocol_version = protocol_version;
	m_clear_rejected_autoclusters = false;
	m_use_resource_request_counts = param_boolean("USE_RESOURCE_REQUEST_COUNTS",true);
	if ( protocol_version == 0 || m_use_resource_request_counts == false ) {
		// Protocol version is 0, and schedd resource request lists were introduced
		// in protocol version 1.  so set m_num_to_fetch to 1 so we use the old
		// protocol of getting one request at a time with this old schedd.
		// Also we must use the old protocol if admin disabled USE_RESOURCE_REQUEST_COUNTS,
		// since the new protocol relies on that.
		m_num_to_fetch = 1;
	} else {
		m_num_to_fetch = param_integer("NEGOTIATOR_RESOURCE_REQUEST_LIST_SIZE");
	}
	errcode = 0;
	current_autocluster = -1;
	resource_request_count = 0;
	resource_request_offers = 0;
}

ResourceRequestList::~ResourceRequestList()
{
	ClassAd *front;

	while( !m_ads.empty() ) {
		front = m_ads.front();
		m_ads.pop_front();
		delete front;
	}
}

bool
ResourceRequestList::needsEndNegotiate() const
{
	return m_send_end_negotiate;
}


bool
ResourceRequestList::needsEndNegotiateNow() const
{
	return m_send_end_negotiate_now;
}


bool
ResourceRequestList::getRequest(ClassAd &request, int &cluster, int &proc, int &autocluster,
								ReliSock* const sock, int skip_jobs)
{
	errcode = 0;	// clear errcode
	cluster = -1;
	proc = -1;
	autocluster = -1;

	// If the previous slot we matched to was partitionable, and we
	// estimate that the schedd will fit > 1 job in it, skip over
	// those jobs, and assume the schedd will take care of them
	resource_request_offers += skip_jobs;

	if ( resource_request_count > resource_request_offers ) {
		// No need to go to the schedd to ask for another resource request,
		// since we have not yet handed out the number of requested matches
		// for the request we already have cached.
		request = cached_resource_request;
	} else {
			// The prefetch of RRLs saw a NO_MORE_JOBS from the schedd.
			// Hence, once m_ads is empty and we run out or resource_request_count,
			// then we ought to return false.
			//
			// In this case, we never ask the schedd to send job info, so we must
			// send an explicit END_NEGOTIATE to inform it we are done.
		if (m_send_end_negotiate && m_ads.empty())
		{
			return false;
		}

		// We used up our cached_resource_request, so no we need to grab the
		// next request from our list m_ads.
		
		resource_request_offers = 0;
		resource_request_count = 0;

		// Pull off front of m_ads list into *front, skipping any
		// requests associated with rejected autoclusters, and going
		// back over the wire to the schedd for more ads as needed.  
		// If there are no more requests, return false.
		ClassAd *front = NULL;		
		while ( front == NULL ) {
			if ( m_ads.empty() ) {
				// The prefetch of RRLs saw a NO_MORE_JOBS from the schedd.
				// Don't ask the schedd for more jobs once m_ads is empty.
				// Keep checking this, as we may be throwing out ads in
				// m_ads because they're in rejected autoclusters.
				if ( m_send_end_negotiate ) {
					return false;
				}
				// No more requests stashed in our list, so go over the wire
				// to the schedd and ask for more.
				if ( m_clear_rejected_autoclusters ) {
					m_rejected_auto_clusters.clear();
					m_clear_rejected_autoclusters = false;
				}
				TryStates result = fetchRequestsFromSchedd(sock, true);
				if ((result != RRL_DONE) && (result != RRL_NO_MORE_JOBS))
				{
					// note: Do not set errcode here, it had better be set
					// appropriately by fetchRequestFromSchedd()
					ASSERT(errcode > 0);
					return false;
				}
				if ( m_ads.empty() ) {
					// if m_ads is still empty, schedd must have nothing
					// more to give.  
					return false;
				}
			}
			ASSERT( !m_ads.empty() );
			front = m_ads.front();
			m_ads.pop_front();
			MSC_SUPPRESS_WARNING(6011) // code analysis thinks front may be null, code analysis is wrong
			front->LookupInteger(ATTR_AUTO_CLUSTER_ID,current_autocluster);
			if ( m_rejected_auto_clusters.find(current_autocluster) != 
					m_rejected_auto_clusters.end() ) 
			{
				// this request has an autocluster that has been rejected.
				// skip it.
				delete front;
				front = NULL;
			}
		}
		// we have a result in *front; copy it into request and deallocate front
		request = *front;
		delete front;
		front = NULL;

		if ( m_use_resource_request_counts ) {
			request.LookupInteger(ATTR_RESOURCE_REQUEST_COUNT,resource_request_count);
			if (resource_request_count > 0) {
				cached_resource_request = request;
			}
		}
	}	// end of going over wire to ask schedd for request

	//
	// If we made it here, "request" has a copy of the next request ad.
	//

	// fill in cluster, proc, and autocluster function parameters
	if (!request.LookupInteger (ATTR_CLUSTER_ID, cluster) ||
		!request.LookupInteger (ATTR_PROC_ID, proc))
	{
		dprintf (D_ALWAYS, "    Could not get %s and %s from request\n",
				ATTR_CLUSTER_ID, ATTR_PROC_ID);
		errcode = __LINE__;
		return false;
	}
	autocluster = current_autocluster;

	dprintf(D_ALWAYS, "    Request %05d.%05d: autocluster %d (request count %d of %d)\n", cluster, proc,
		autocluster, resource_request_offers+1,resource_request_count);

	return true;
}

void
ResourceRequestList::noMatchFound()
{
	// No match was found for the current request, so we want to stop using
	// this autocluster.
	
	// Stop re-using current autocluster
	resource_request_count = 0;

	if ( current_autocluster < 0 ) {
		// This shouldn't really happen, but just in case...
		return;
	}
	
	// Don't use any other instances of current autocluster in the list.
	// However, if we are always only fetching one request at a time (i.e.
	// because we are talking to an older schedd), no need to waste time
	// collecting up a set of rejected autoclusters as the schedd will keep
	// track of this.
	if ( m_num_to_fetch > 1 ) {
		m_rejected_auto_clusters.insert( current_autocluster );
	}
}

ResourceRequestList::TryStates
ResourceRequestList::tryRetrieve(ReliSock *const sock)
{
	TryStates result = fetchRequestsFromSchedd(sock, false);
		// Protocol version 1 requires an END_NEGOTIATE after NO_MORE_JOBS if *any* RRLs were received
		//   - If NO_MORE_JOBS was sent without any RRLs, then END_NEGOTIATE is not needed
		//
		// Protocol version 0 does not require any immediate end negotiate -- NO_MORE_JOBS indicates
		// the schedd is done.
	m_send_end_negotiate_now = (result == RRL_DONE) || ((result == RRL_NO_MORE_JOBS) && !m_ads.empty() && (m_protocol_version==1));
		// This indicates that we already saw NO_MORE_JOBS; hence, when using the cached RRL, we'll
		// never request jobs.
	m_send_end_negotiate = (result == RRL_NO_MORE_JOBS);
	return result;
}

ResourceRequestList::TryStates
ResourceRequestList::fetchRequestsFromSchedd(ReliSock* const sock, bool blocking)
{
	int reply;
	ClassAd *request_ad;

	// go over the wire and ask the schedd for the request
	int sleepy = param_integer("NEG_SLEEP", 0);
		//  This sleep is useful for any testing that calls for a
		//  long negotiation cycle, please do not remove
		//  it. Examples of such testing are the async negotiation
		//  protocol w/ schedds and reconfig delaying until after
		//  a negotiation cycle. -matt 21 mar 2012
	if ( sleepy ) {
		dprintf(D_ALWAYS, "begin sleep: %d seconds\n", sleepy);
		sleep(sleepy); // TODD DEBUG - allow schedd to do other things
		dprintf(D_ALWAYS, "end sleep: %d seconds\n", sleepy);
	}


	ASSERT(m_num_to_fetch > 0);	

	if (!m_requests_to_fetch)
	{
		if ( m_num_to_fetch == 1 ) {
			dprintf (D_FULLDEBUG, 
				"    Sending SEND_JOB_INFO/eom\n");
			sock->encode();
			if (!sock->put(SEND_JOB_INFO) || !sock->end_of_message())
			{
				dprintf (D_ALWAYS, 
					"    Failed to send SEND_JOB_INFO/eom\n");
				errcode = __LINE__;
				return RRL_ERROR;
			}
		} else {
			dprintf (D_FULLDEBUG, 
				"    Sending SEND_RESOURCE_REQUEST_LIST/%d/eom\n",m_num_to_fetch);
			sock->encode();
			if (!sock->put(SEND_RESOURCE_REQUEST_LIST) || !sock->put(m_num_to_fetch) || 
				!sock->end_of_message())
			{
				dprintf (D_ALWAYS, 
					"    Failed to send SEND_RESOURCE_REQUEST_LIST/%d/eom\n",m_num_to_fetch);
				errcode = __LINE__;
				return RRL_ERROR;
			}
		}
		m_requests_to_fetch = m_num_to_fetch;
	}

	while (m_requests_to_fetch > 0) {
		// 2b.  the schedd may either reply with JOB_INFO or NO_MORE_JOBS
		dprintf (D_FULLDEBUG, "    Getting reply from schedd ...\n");
		sock->decode();
		int retval;
		bool read_would_block;
		{
			BlockingModeGuard guard(sock, !blocking);
			retval = sock->get(reply);
			read_would_block = sock->clear_read_block_flag();
		}
		if (read_would_block)
		{
			dprintf(D_NETWORK, "Resource request would block.\n");
			return RRL_CONTINUE;
		}
		m_requests_to_fetch--;
		if (!retval)
		{
			dprintf (D_ALWAYS, "    Failed to get reply from schedd\n");
			sock->end_of_message ();
			errcode = __LINE__;
			m_requests_to_fetch = 0;
			return RRL_ERROR;
		}

		// 2c.  if the schedd replied with NO_MORE_JOBS, cleanup and quit
		if (reply == NO_MORE_JOBS)
		{
			dprintf (D_ALWAYS, "    Got NO_MORE_JOBS;  schedd has no more requests\n");
			sock->end_of_message ();
			// Note: do NOT set errcode here, as this is not an error condition
			m_requests_to_fetch = 0;
			return RRL_NO_MORE_JOBS;
		}
		else
		if (reply != JOB_INFO)
		{
			// something goofy
			dprintf(D_ALWAYS,"    Got illegal command %d from schedd\n",reply);
			sock->end_of_message ();
			errcode = __LINE__;
			m_requests_to_fetch = 0;
			return RRL_ERROR;
		}

		// 2d.  get the request
		dprintf (D_FULLDEBUG,"    Got JOB_INFO command; getting classad/eom\n");
		request_ad = getClassAd(sock);	// allocates a new ClassAd on success, NULL on fail
		if ( request_ad==NULL || !sock->end_of_message() )
		{
			dprintf(D_ALWAYS, "    JOB_INFO command not followed by ad/eom\n");
			sock->end_of_message();
			errcode = __LINE__;
			m_requests_to_fetch = 0;
			return RRL_ERROR;
		}
		m_ads.push_back(request_ad);
	}

	return RRL_DONE;
}

