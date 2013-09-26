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
ScheddNegotiate::sendJobInfo(Sock *sock)
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
	m_current_job_ad.Assign(ATTR_WANT_MATCH_DIAGNOSTICS, true);

		// Send the ad to the negotiator
	if( !putClassAd(sock, m_current_job_ad) ) {
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
	case REJECTED_WITH_REASON:
		scheduler_handleJobRejected( m_current_job_id, m_reject_reason.c_str() );
		m_jobs_rejected++;
		setAutoClusterRejected( m_current_auto_cluster_id );
		nextJob();
		break;

	case SEND_JOB_INFO:
		if( !sendJobInfo(sock) ) {
				// We failed to talk to the negotiator, so close the socket.
			return MESSAGE_FINISHED;
		}
		break;

	case PERMISSION_AND_AD: {

		// If the slot we matched is partitionable, edit it
		// so it will look like the resulting dynamic slot. 
		// NOTE: Seems like we no longer need to do this here,
		// since we also do the fixup at claim time in
		// contactStartd().  - Todd 1/12 <tannenba@cs.wisc.edu>
		if( !fixupPartitionableSlot(&m_current_job_ad,&m_match_ad) )
		{
			nextJob();
			break;
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
	}


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
