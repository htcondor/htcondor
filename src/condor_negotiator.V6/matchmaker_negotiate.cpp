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
ResourceRequestList::getRequest(ClassAd &request, int &cluster, int &proc, int &autocluster,
								ReliSock* const sock)
{
	errcode = 0;	// clear errcode
	cluster = -1;
	proc = -1;
	autocluster = -1;

	// 2a.  ask for job information
	if ( resource_request_count > ++resource_request_offers ) {
		// No need to go to the schedd to ask for another resource request,
		// since we have not yet handed out the number of requested matches
		// for the request we already have cached.
		request = cached_resource_request;
	} else {
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
				// No more requests stashed in our list, so go over the wire
				// to the schedd and ask for more.
				if ( m_clear_rejected_autoclusters ) {
					m_rejected_auto_clusters.clear();
					m_clear_rejected_autoclusters = false;
				}
				if (!fetchRequestsFromSchedd(sock))
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

	dprintf(D_ALWAYS, "    Request %05d.%05d:  (request count %d of %d)\n", cluster, proc,
		resource_request_offers+1,resource_request_count);

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

bool
ResourceRequestList::fetchRequestsFromSchedd(ReliSock* const sock)
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

	if ( m_num_to_fetch == 1 ) {
		dprintf (D_FULLDEBUG, 
			"    Sending SEND_JOB_INFO/eom\n");
		sock->encode();
		if (!sock->put(SEND_JOB_INFO) || !sock->end_of_message())
		{
			dprintf (D_ALWAYS, 
				"    Failed to send SEND_JOB_INFO/eom\n");
			errcode = __LINE__;
			return false;
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
			return false;
		}
	}

	for (int i=0; i<m_num_to_fetch; i++) {
		// 2b.  the schedd may either reply with JOB_INFO or NO_MORE_JOBS
		dprintf (D_FULLDEBUG, "    Getting reply from schedd ...\n");
		sock->decode();
		if (!sock->get (reply))
		{
			dprintf (D_ALWAYS, "    Failed to get reply from schedd\n");
			sock->end_of_message ();
			errcode = __LINE__;
			return false;
		}

		// 2c.  if the schedd replied with NO_MORE_JOBS, cleanup and quit
		if (reply == NO_MORE_JOBS)
		{
			dprintf (D_ALWAYS, "    Got NO_MORE_JOBS;  schedd has no more requests\n");
			sock->end_of_message ();
			// Note: do NOT set errcode here, as this is not an error condition
			return true;
		}
		else
		if (reply != JOB_INFO)
		{
			// something goofy
			dprintf(D_ALWAYS,"    Got illegal command %d from schedd\n",reply);
			sock->end_of_message ();
			errcode = __LINE__;
			return false;
		}

		// 2d.  get the request
		dprintf (D_FULLDEBUG,"    Got JOB_INFO command; getting classad/eom\n");
		request_ad = getClassAd(sock);	// allocates a new ClassAd on success, NULL on fail
		if ( request_ad==NULL || !sock->end_of_message() )
		{
			dprintf(D_ALWAYS, "    JOB_INFO command not followed by ad/eom\n");
			sock->end_of_message();
			errcode = __LINE__;
			return false;
		}
		m_ads.push_back(request_ad);
	}

	return true;
}

#if 0
	RequestListManager::~RequestListManager()
	{
		clear();
	}

	void
	RequestListManager::clear()
	{
		m_request_lists_map.clear();	
	}

	ResourceRequestList*
	RequestListManager::getRequestList(const MyString &key)
	{
		if ( m_request_lists_map.find(key) == m_request_lists_map.end() ) {
			// key is not in the map, create a new object on the heap
			ResourceRequestList *entry;
			entry = new ResourceRequestList(0);
			m_request_lists_map[key] = entry;
			return entry;
		} else {
			return m_request_lists_map[key].get();
		}
	}
#endif
