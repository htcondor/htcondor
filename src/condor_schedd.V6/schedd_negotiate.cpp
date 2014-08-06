/***************************************************************
 *
 * Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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
#include "schedd_negotiate.h"

ResourceRequestCluster::ResourceRequestCluster(int auto_cluster_id):
	m_auto_cluster_id(auto_cluster_id)
{
}

void
ResourceRequestCluster::addJob(PROC_ID job_id)
{
	m_job_ids.push_back( job_id );
}

bool
ResourceRequestCluster::popJob(PROC_ID &job_id)
{
	if( m_job_ids.empty() ) {
		return false;
	}
	job_id = m_job_ids.front();
	m_job_ids.pop_front();
	return true;
}

ResourceRequestList::~ResourceRequestList()
{
	while( !empty() ) {
		ResourceRequestCluster *cluster = front();
		pop_front();
		delete cluster;
	}
}

ScheddNegotiate::ScheddNegotiate
(
	int cmd,
	ResourceRequestList *jobs,
	char const *owner,
	char const *remote_pool
):
	DCMsg(cmd),
	m_jobs(jobs),
	m_current_resources_requested(1),
	m_current_resources_delivered(0),
	m_owner(owner ? owner : ""),
	m_remote_pool(remote_pool ? remote_pool : ""),
	m_current_auto_cluster_id(-1),
	m_jobs_rejected(0),
	m_jobs_matched(0),
	m_num_resource_reqs_sent(0),
	m_num_resource_reqs_to_send(0),
	m_negotiation_finished(false),
	m_operation(0)
{
	m_current_job_id.cluster = -1;
	m_current_job_id.proc = -1;
}

ScheddNegotiate::~ScheddNegotiate()
{
	delete m_jobs;
}

void
ScheddNegotiate::negotiate(Sock *sock)
{
		// The negotiator is talking to us on sock.  The initial
		// negotiation header has already been read and handled.  Now
		// it is our job to do the inner negotiation loop, which
		// consists of asynchronously reading messages from the
		// negotiator and sending responses until this round of
		// negotiation is done.

		// This will eventually call ScheddNegotiate::readMsg().
	classy_counted_ptr<DCMessenger> messenger = new DCMessenger(sock);
	messenger->readMsg( this, sock );
}

char const *
ScheddNegotiate::getOwner()
{
	return m_owner.c_str();
}

char const *
ScheddNegotiate::getRemotePool()
{
	if( m_remote_pool.empty() ) {
		return NULL;
	}
	return m_remote_pool.c_str();
}

bool
ScheddNegotiate::nextJob()
{
	if ( m_num_resource_reqs_sent > 0 && m_num_resource_reqs_to_send == 0 ) {
		// If we have already sent off a list of resource requests via
		// sendResourceRequestList(), and we are not currently being asked
		// by the negotiator to send more requests (or jobs), then
		// we want nextJob() to essentially be a no-op here.
		// In other words, when doing resource request lists, we want
		// all the calls to nextJob() to be ignored unless we are
		// in the process of sending a resource request list.
		// Note that if we are talking to anything earlier than a v8.3.0
		// negotiator, m_num_resource_reqs_sent will always be 0.
		return true;
	}

	while( !m_jobs->empty() ) {
		ResourceRequestCluster *cluster = m_jobs->front();
		ASSERT( cluster );

		m_current_auto_cluster_id = cluster->getAutoClusterId();

		if( !getAutoClusterRejected(m_current_auto_cluster_id) ) {
			while( cluster->popJob(m_current_job_id) ) {
				if( !scheduler_skipJob(m_current_job_id) ) {

					if( !scheduler_getJobAd( m_current_job_id, m_current_job_ad ) )
					{
						dprintf(D_FULLDEBUG,
							"skipping job %d.%d because it no longer exists\n",
							m_current_job_id.cluster,m_current_job_id.proc);
					}
					else {
						// Insert the number of jobs remaining in this
						// resource request cluster into the ad - the negotiator
						// may use this information to give us more than one match
						// at a time.
						// [[Future optimization idea: there may be jobs in this resource request
						// cluster that no longer exist in the queue; perhaps we should
						// iterate through them and make sure they still exist to prevent
						// asking the negotiator for more resources than we can really
						// use at the moment. ]]
						// - Todd July 2013 <tannenba@cs.wisc.edu>
						int universe = CONDOR_UNIVERSE_MIN;
						m_current_job_ad.LookupInteger(ATTR_JOB_UNIVERSE,universe);
						// For now, do not use request counts with the dedicated scheduler
						if ( universe != CONDOR_UNIVERSE_PARALLEL ) {
							// add one to cluster size to cover the current popped job
							m_current_job_ad.Assign(ATTR_RESOURCE_REQUEST_COUNT,1+cluster->size());
						}

						// Copy attributes from chained parent ad into our copy 
						// so if parent is deleted before we finish negotiation,
						// we don't crash trying to access a deleted parent ad.
						m_current_job_ad.ChainCollapse();
						return true;
					}
				}
			}
		}

		m_jobs->pop_front();
		delete cluster;
	}

	m_current_auto_cluster_id = -1;
	m_current_job_id.cluster = -1;
	m_current_job_id.proc = -1;

	return false;
}

bool
ScheddNegotiate::getAutoClusterRejected(int auto_cluster_id)
{
	return m_rejected_auto_clusters.find(auto_cluster_id) !=
	       m_rejected_auto_clusters.end();
}

void
ScheddNegotiate::setAutoClusterRejected(int auto_cluster_id)
{
	m_rejected_auto_clusters.insert( auto_cluster_id );
}

bool
ScheddNegotiate::fixupPartitionableSlot(ClassAd *job_ad, ClassAd *match_ad)
{
	ASSERT( match_ad );
	ASSERT( job_ad );

	int is_partitionable = 0;
	match_ad->LookupBool(ATTR_SLOT_PARTITIONABLE,is_partitionable);
	if (!is_partitionable) {
		return true;
	}

		// Grab some attribute values so we can print out nicer debug messages.
	std::string slot_name_buf;
	match_ad->LookupString(ATTR_NAME,slot_name_buf);
	char const *slot_name = slot_name_buf.c_str();
	PROC_ID job_id;
	job_id.cluster = -1;
	job_id.proc = -1;
	job_ad->LookupInteger(ATTR_CLUSTER_ID,job_id.cluster);
	job_ad->LookupInteger(ATTR_PROC_ID,job_id.proc);


		// We want to avoid re-using a claim to a partitionable slot
		// for jobs that do not fit the dynamically created
		// slot. Since we simply compare requirements in
		// FindRunnableJob we need to make sure match_ad accurately
		// reflects the dynamic slot. So, temporarily (Condor-style),
		// we will massage match_ad to look like the dynamic slot will
		// once the claim is requested.

	bool result = true;
	int cpus, memory, disk;

	cpus = 1;
	job_ad->EvalInteger(ATTR_REQUEST_CPUS, match_ad, cpus);
	match_ad->Assign(ATTR_CPUS, cpus);

	memory = -1;
	if (job_ad->EvalInteger(ATTR_REQUEST_MEMORY, match_ad, memory)) {
		match_ad->Assign(ATTR_MEMORY, memory);
	} else {
		dprintf(D_ALWAYS, "No memory request in job %d.%d, skipping match to partitionable slot %s\n", job_id.cluster, job_id.proc, slot_name);
		result = false;
	}

	if (job_ad->EvalInteger(ATTR_REQUEST_DISK, match_ad, disk)) {
		float total_disk = disk;
		match_ad->LookupFloat(ATTR_TOTAL_DISK, total_disk);
		disk = (MAX((int) ceil((disk / total_disk) * 100), 1)) *
			int(total_disk/100.0);
		match_ad->Assign(ATTR_DISK, disk);
	} else {
		dprintf(D_ALWAYS, "No disk request in job %d.%d, skipping match to partitionable slot %s\n", job_id.cluster, job_id.proc, slot_name);
		result = false;
	}

	if( result ) {
		// If successful, remove attribute claiming this slot is partitionable
		// and instead mark it as dynamic. This is what the startd does. Plus,
		// removing the paritionable attribute means if we happen to call
		// fixupPartitionableSlot again in the future on this same ad, we 
		// won't mangle it a second time.
		match_ad->Assign(ATTR_SLOT_DYNAMIC, true);
		match_ad->Assign(ATTR_SLOT_PARTITIONABLE,false);
		dprintf(D_FULLDEBUG,
				"Partitionable slot %s adjusted for job %d.%d: "
				"cpus = %d, memory = %d, disk = %d\n",
				slot_name, job_id.cluster, job_id.proc, cpus, memory, disk);
	}

	return result;
}

bool
ScheddNegotiate::sendResourceRequestList(Sock *sock)
{
	while (m_num_resource_reqs_to_send > 0) {

		nextJob();

		if ( !sendJobInfo(sock, true) ) {
			return false;
		}

		// If m_negotiation_finished==true, then no more jobs to send. But
		// if we already sent some jobs in response to this request, we
		// don't want to consider the negotitation finished since we still want
		// to receive responses (e.g. matches) back from the negotiator.
		if ( m_negotiation_finished ) {
			if (m_num_resource_reqs_sent > 0 ) {
				m_negotiation_finished = false;
			}
			break;
		}

		// When we call sendJobInfo next at the top of the loop,
		// we don't want it to send all the individual jobs in the current cluster since
		// we already sent an ad with a resource_request_count.  So we want
		// to skip ahead to the next cluster.
		if ( !m_jobs->empty() ) {
			ResourceRequestCluster *cluster = m_jobs->front();
			m_jobs->pop_front();
			delete cluster;
		}

		m_num_resource_reqs_sent++;
		m_num_resource_reqs_to_send--;
	}

	// Set m_num_resource_reqs_to_send to zero, as we are not sending
	// any more reqs now, and this counter is inspected in nextJob()
	m_num_resource_reqs_to_send = 0; 

	return true;
}


bool
ScheddNegotiate::sendJobInfo(Sock *sock, bool just_sig_attrs)
{
		// The Negotiator wants us to send it a job. 

	sock->encode();

	if( m_current_job_id.cluster == -1 && !nextJob() ) {
		if( !sock->snd_int(NO_MORE_JOBS,TRUE) ) {
			dprintf( D_ALWAYS, "Can't send NO_MORE_JOBS to mgr\n" );
			return false;
		}
		m_negotiation_finished = true;
		return true;
	}

	if( !sock->put(JOB_INFO) ) {
		dprintf( D_ALWAYS, "Can't send JOB_INFO to mgr\n" );
		return false;
	}

		// request match diagnostics
		// 0 = no match diagnostics
		// 1 = match diagnostics string
		// 2 = match diagnostics string decorated w/ autocluster + jobid
	m_current_job_ad.Assign(ATTR_WANT_MATCH_DIAGNOSTICS, (int) 2);

		// Send the ad to the negotiator
	int putad_result = 0;
	std::string auto_cluster_attrs;
	if ( just_sig_attrs &&
		 m_current_job_ad.LookupString(ATTR_AUTO_CLUSTER_ATTRS, auto_cluster_attrs) )
	{
		// don't send the entire job ad; just send significant attrs
		classad::References sig_attrs;

		StringTokenIterator list(auto_cluster_attrs);
		const std::string *attr;
		while ((attr = list.next_string())) { sig_attrs.insert(*attr); }

		// besides significant attrs, we also always want to send these attrs cuz
		// the matchmaker explicitly looks for them (for dprintfs or whatever).
		sig_attrs.insert(ATTR_OWNER);
		sig_attrs.insert(ATTR_CLUSTER_ID);
		sig_attrs.insert(ATTR_PROC_ID);
		sig_attrs.insert(ATTR_RESOURCE_REQUEST_COUNT);
		sig_attrs.insert(ATTR_GLOBAL_JOB_ID);
		sig_attrs.insert(ATTR_AUTO_CLUSTER_ID);
		sig_attrs.insert(ATTR_WANT_MATCH_DIAGNOSTICS);
		sig_attrs.insert(ATTR_WANT_CLAIMING);  // used for Condor-G matchmaking
		// ship it!
		putad_result = putClassAd(sock, m_current_job_ad, 0, &sig_attrs);
	} else {
		// send the entire classad.  perhaps we are doing this because the
		// ad does not have ATTR_AUTO_CLUSTER_ATTRS defined for some reason,
		// or perhaps we are doing this because we were explicitly told to do so.
		putad_result = putClassAd(sock, m_current_job_ad);
	}
	if( !putad_result ) {
		dprintf( D_ALWAYS,
				 "Can't send job ad to mgr\n" );
		sock->end_of_message();
		return false;
	}
	if( !sock->end_of_message() ) {
		dprintf( D_ALWAYS,
				 "Can't send job eom to mgr\n" );
		return false;
	}

	m_current_resources_delivered = 0;
	m_current_resources_requested = 1;
	m_current_job_ad.LookupInteger(ATTR_RESOURCE_REQUEST_COUNT,m_current_resources_requested);

	dprintf( D_FULLDEBUG,
			 "Sent job %d.%d (autocluster=%d resources_requested=%d) to the negotiator\n",
			 m_current_job_id.cluster, m_current_job_id.proc,
			 m_current_auto_cluster_id, m_current_resources_requested );
	return true;
}

DCMsg::MessageClosureEnum
ScheddNegotiate::messageReceived( DCMessenger *messenger, Sock *sock )
{
		// This is called when readMsg() returns true.
		// Now carry out the negotiator's request that we just read.

	switch( m_operation ) {

	case REJECTED:
		m_reject_reason = "Unknown reason";

	case REJECTED_WITH_REASON: {
		// To support resource request lists, the
		// reject reason may end with "...|autocluster|cluster.proc|"
		// if so, reset m_current_auto_cluster_id and m_current_job_id
		// with the values contained in the reject reason, and truncate
		// this information out of m_reject_reason.
		int pos = m_reject_reason.FindChar('|');
		if ( pos >= 0 ) {
			m_reject_reason.Tokenize();
			/*const char *reason =*/ m_reject_reason.GetNextToken("|",false);
			const char *ac = m_reject_reason.GetNextToken("|",false);
			const char *jobid = m_reject_reason.GetNextToken("|",false);
			if (ac && jobid) {
				int rr_cluster, rr_proc;
				m_current_auto_cluster_id = atoi(ac);
				StrToProcId(jobid,rr_cluster,rr_proc);
				if (rr_cluster != m_current_job_id.cluster || rr_proc != m_current_job_id.proc) {
					m_current_resources_delivered = 0;
				}
				m_current_job_id.cluster = rr_cluster;
				m_current_job_id.proc = rr_proc;
			}
			m_reject_reason.setChar(pos,'\0');	// will truncate string at pos
		}
		scheduler_handleJobRejected( m_current_job_id, m_reject_reason.c_str() );
		m_jobs_rejected++;
		setAutoClusterRejected( m_current_auto_cluster_id );
		nextJob();
		break;
	}

	case SEND_JOB_INFO:
		m_num_resource_reqs_sent = 0;  // clear counter of reqs sent this round
		if( !sendJobInfo(sock) ) {
				// We failed to talk to the negotiator, so close the socket.
			return MESSAGE_FINISHED;
		}
		break;

	case SEND_RESOURCE_REQUEST_LIST:
		m_num_resource_reqs_sent = 0; // clear counter of reqs sent this round
		if( !sendResourceRequestList(sock) ) {
				// We failed to talk to the negotiator, so close the socket.
			return MESSAGE_FINISHED;
		}
		break;

	case PERMISSION_AND_AD: {

		// When using request lists, one single
		// "m_current_job_id" is kinda meaningless if we just sent a whole
		// pile of jobs to the negotiator.  So we want to 
		// reset m_current_job_id with the job id info embedded in the offer 
		// that comes back from the negotiator (if it exists).  This will
		// happen with an 8.3.0+ negotiator, and is needed when using
		// resource request lists.  
		int rr_cluster = -1;
		int rr_proc = -1;
		m_match_ad.LookupInteger(ATTR_RESOURCE_REQUEST_CLUSTER, rr_cluster);
		m_match_ad.LookupInteger(ATTR_RESOURCE_REQUEST_PROC, rr_proc);
		if (rr_cluster != -1 && rr_proc != -1) {
			if (rr_cluster != m_current_job_id.cluster || rr_proc != m_current_job_id.proc) {
				m_current_resources_delivered = 0;
			}
			m_current_job_id.cluster = rr_cluster;
			m_current_job_id.proc = rr_proc;
		}

		m_current_resources_delivered++;

		std::string slot_name_buf;
		m_match_ad.LookupString(ATTR_NAME,slot_name_buf);
		char const *slot_name = slot_name_buf.c_str();

		int offline = false;
		m_match_ad.EvalBool(ATTR_OFFLINE,NULL,offline);

		if( offline ) {
			dprintf(D_ALWAYS,"Job %d.%d (delivered=%d) matched to offline machine %s.\n",
					m_current_job_id.cluster,m_current_job_id.proc,m_current_resources_delivered,slot_name);
			nextJob();
			break;
		}

		if( scheduler_handleMatch(m_current_job_id,m_claim_id.c_str(),m_match_ad,slot_name) )
		{
			m_jobs_matched++;
		}

		nextJob();

		break;
	}

	case END_NEGOTIATE:
		dprintf( D_ALWAYS, "Lost priority - %d jobs matched\n",
				 m_jobs_matched );

		m_negotiation_finished = true;
		break;

	default:
		EXCEPT("should never get here (negotiation op %d)",m_operation);

	} // end of switch on m_operation

	if( m_negotiation_finished ) {
			// the following function takes ownership of sock
		scheduler_handleNegotiationFinished( sock );
		sock = NULL;
	}
	else {
			// wait for negotiator to write a response
		messenger->startReceiveMsg( this, sock );
	}

		// By returning MESSAGE_CONTINUING, we tell messenger not to
		// close the socket.  Either we have finished negotiating and
		// sock has been taken care of by the scheduler (e.g. by
		// registering it to wait for the next NEGOTIATE command), or
		// we are not yet done with negotiating and we are waiting for
		// the next operation within the current negotiation round.
	return MESSAGE_CONTINUING;
}

bool
ScheddNegotiate::writeMsg( DCMessenger * /*messenger*/, Sock * /*sock*/ )
{
	EXCEPT("this should never be called");
	return false;
}

bool
ScheddNegotiate::readMsg( DCMessenger * /*messenger*/, Sock *sock )
{
		// Get the negotiator's request.
		// Note that end_of_message() is handled by our caller.

	if( !sock->code(m_operation) ) {
		dprintf( D_ALWAYS, "Can't receive request from negotiator\n" );
		return false;
	}

	switch( m_operation ) {
	case REJECTED_WITH_REASON:
		if( !sock->code(m_reject_reason) ) {
			dprintf( D_ALWAYS,
					 "Can't receive reject reason from negotiator\n" );
			return false;
		}
		break;

	case REJECTED:
		break;

	case SEND_JOB_INFO:
		break;

	case SEND_RESOURCE_REQUEST_LIST:
		if( !sock->code(m_num_resource_reqs_to_send) ) {
			dprintf( D_ALWAYS,
					 "Can't receive num_resource_reqs_to_send from negotiator\n" );
			return false;
		}
		break;

	case PERMISSION:
			// No negotiator since 7.1.3 should ever send this
			// command, and older ones should not send it either,
			// since we advertise WantResAd=True.
		dprintf( D_ALWAYS, "Negotiator sent PERMISSION rather than expected PERMISSION_AND_AD!  Aborting.\n");
		return false;
		break;

	case PERMISSION_AND_AD: {
		char *claim_id = NULL;
		if( !sock->get_secret(claim_id) || !claim_id ) {
			dprintf( D_ALWAYS,
					 "Can't receive ClaimId from negotiator\n" );
			return false;
		}
		m_claim_id = claim_id;
		free( claim_id );

		m_match_ad.Clear();

			// get startd ad from negotiator
		if( !getClassAd(sock, m_match_ad) ) {
			dprintf( D_ALWAYS,
					 "Can't get my match ad from negotiator\n" );
			return false;
		}
#if defined(ADD_TARGET_SCOPING)
		m_match_ad.AddTargetRefs( TargetJobAttrs );
#endif

		break;
	}
	case END_NEGOTIATE:
		break;
	default:
		dprintf( D_ALWAYS, "Got unexpected request (%d)\n", m_operation );
		return false;
	}

	return true;
}

bool ScheddNegotiate::getSatisfaction() {
	if( m_jobs_rejected > 0 ) {
		return false;
	}

		// no jobs were explicitly rejected, but did negotiation end
		// before we presented all of our jobs?
	if( m_current_job_id.cluster == -1 ) {
		nextJob();
	}

	if( m_current_job_id.cluster == -1 ) {
		return true; // no more jobs
	}
	return false;
}
