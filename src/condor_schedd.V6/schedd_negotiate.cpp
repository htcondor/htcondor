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


static int next_negotiate_instance_id = 1;

ScheddNegotiate::ScheddNegotiate
(
	int cmd,
	ResourceRequestList *jobs,
	char const *owner,
	char const *remote_pool
):
	DCMsg(cmd),
	m_jobs(jobs),
	m_owner(owner ? owner : ""),
	m_remote_pool(remote_pool ? remote_pool : ""),
	m_current_auto_cluster_id(-1),
	m_jobs_rejected(0),
	m_jobs_matched(0),
	m_num_resource_reqs_sent(0),
	m_num_resource_reqs_to_send(0),
	m_negotiation_finished(false),
	m_first_rrl_request(true),
	m_operation(0)
{
	m_current_job_id.cluster = -1;
	m_current_job_id.proc = -1;
	m_instance = next_negotiate_instance_id++;
}

ScheddNegotiate::~ScheddNegotiate()
{
	delete m_jobs;
	delete m_reject_ad; m_reject_ad = nullptr;
}

void
ScheddNegotiate::setMatchCaps(std::string_view caps)
{
	for (auto & str : StringTokenIterator(caps)) {
		if (YourStringNoCase("MatchDiag3") == str) { m_can_do_match_diag_3 = true; }
		else if (YourStringNoCase("Dye") == str) { m_can_do_match_dye = true; }
	}
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
	dprintf(D_DYE, "%08x_%06d:%s\tNEGOTIATE\t%s rr=%d\n",
		sock->getUniqueId(), m_instance,
		m_owner.c_str(), m_remote_pool.c_str(), m_jobs ? (int)m_jobs->size() : 0);

		// This will eventually call ScheddNegotiate::readMsg().
	classy_counted_ptr<DCMessenger> messenger = new DCMessenger(sock);
	messenger->readMsg( this, sock );
}

char const *
ScheddNegotiate::getMatchUser()
{
	return m_owner.c_str();
}

char const *
ScheddNegotiate::getRemotePool()
{
	if( m_remote_pool.empty() ) {
		return nullptr;
	}
	return m_remote_pool.c_str();
}

char const *
ScheddNegotiate::getNegotiatorName()
{
	return m_negotiator_name.c_str();
}

// this is in qmgmt.cpp...
extern JobQueueJob* GetJobAd(const PROC_ID& jid);

// flatten/expand the items in the list so that each ResourceRequestCluster
// holds only a single job.  We do this so that the old negotiator prototol
// that doesn't understand resource request lists can be fed a single job at a time
//
void ResourceRequestList::flatten()
{
	if (largest_cluster <= 1) return;

	std::deque<ResourceRequestCluster> tmp;
	items.swap(tmp);

	largest_cluster = 0;
	for (auto & cluster : tmp) {
		int id = cluster.getAutoClusterId();
		for (auto & jid : cluster) {
			items.emplace_back(id).addJob(jid);
			largest_cluster = MAX(largest_cluster, items.back().size());
		}
	}
}

// when the negotiator sends a SEND_JOB_INFO instead of SEND_RESOURCE_REQUEST_LIST
// then it doesn't want to know about resource request lists, so we re-write
// the ResourceRequestList items so that each only holds a single job.
void ScheddNegotiate::notSendingResourceRequests()
{
	if ( ! m_jobs || m_jobs->empty()) return;
	m_jobs->flatten();
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

	while( !m_jobs->empty() && m_jobs_can_offer ) {
		ResourceRequestCluster & cluster = m_jobs->front();

		m_current_auto_cluster_id = cluster.getAutoClusterId();

		if( !getAutoClusterRejected(m_current_auto_cluster_id) ) {
			size_t clusterSize = cluster.size();
			for (auto & jid : cluster) {
				--clusterSize; // decrement as we iterate so we know how many jobs remain
				if (!jid.isJobKey()) continue;
				m_current_job_id = jid;

				const char* because = "";
				bool skip_all = false;
				JobQueueJob * job = GetJobAd(m_current_job_id);
				if ( ! job)
				{
					dprintf(D_MATCH,
						"skipping job %d.%d because it no longer exists\n",
						m_current_job_id.cluster,m_current_job_id.proc);
					continue;
				}

				if( !scheduler_skipJob(job, nullptr, skip_all, because) ) {
					// now get a *copy* of the job into m_current_job_ad. it is still linked to the cluster ad
					// until we call ChainCollapse() on the copy which we do before leaving this method.
					if( !scheduler_getJobAd( m_current_job_id, m_current_job_ad ) )
					{
						dprintf(D_MATCH,
							"skipping job %d.%d because it no longer exists\n",
							m_current_job_id.cluster,m_current_job_id.proc);
					}
					else {
						int count_max = INT_MAX;

						if ( ! scheduler_getRequestConstraints(m_current_job_id, m_current_job_ad, &count_max)) {
							dprintf(D_MATCH,
								"skipping job %d.%d because scheduler_getRequestConstraints returned false\n",
								m_current_job_id.cluster,m_current_job_id.proc);
						}
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
							// resource_count is the remaining un-iterated jobs plus this one.
							int resource_count = 1+clusterSize;
							if (count_max > 0) { resource_count = MIN(resource_count, count_max); }
							if (resource_count > m_jobs_can_offer && (m_jobs_can_offer > 0))
							{
								dprintf(D_FULLDEBUG, "Offering %d jobs instead of %d to the negotiator for this cluster; nearing internal limits (MAX_JOBS_RUNNING, etc).\n", m_jobs_can_offer, resource_count);
								resource_count = m_jobs_can_offer;
							}
							m_jobs_can_offer -= resource_count;
							m_current_job_ad.Assign(ATTR_RESOURCE_REQUEST_COUNT,resource_count);
						}
						else {
							m_jobs_can_offer--;
						}

						// Copy attributes from chained parent ad into our copy 
						// so if parent is deleted before we finish negotiation,
						// we don't crash trying to access a deleted parent ad.
						ChainCollapse(m_current_job_ad);
						return true;
					}
				}
			}
		}

		m_jobs->pop_front();
	}
	if (!m_jobs_can_offer)
	{
		dprintf(D_FULLDEBUG, "Not offering any more jobs to the negotiator because I am nearing the internal limits (MAX_JOBS_RUNNING, etc).\n");
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

	bool is_partitionable = false;
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
	int64_t cpus = 0, memory = 0, disk = 0;

	cpus = 1;
	EvalInteger(ATTR_REQUEST_CPUS, job_ad, match_ad, cpus);
	match_ad->Assign(ATTR_CPUS, cpus);

	memory = -1;
	if (EvalInteger(ATTR_REQUEST_MEMORY, job_ad, match_ad, memory)) {
		match_ad->Assign(ATTR_MEMORY, memory);
	} else {
		dprintf(D_ALWAYS, "No memory request in job %d.%d, skipping match to partitionable slot %s\n", job_id.cluster, job_id.proc, slot_name);
		result = false;
	}

	disk = 1;
	if (EvalInteger(ATTR_REQUEST_DISK, job_ad, match_ad, disk)) {
		double total_disk = disk;
		match_ad->LookupFloat(ATTR_TOTAL_DISK, total_disk);
		disk = (MAX((int64_t) ceil((disk / total_disk) * 1000), 1)) *
			int64_t(total_disk/1000.0);
		match_ad->Assign(ATTR_DISK, disk);
	} else {
		dprintf(D_ALWAYS, "No disk request in job %d.%d, skipping match to partitionable slot %s\n", job_id.cluster, job_id.proc, slot_name);
		result = false;
	}

	std::string res_list_str;
	match_ad->LookupString( ATTR_MACHINE_RESOURCES, res_list_str );

	for (auto& res: StringTokenIterator(res_list_str)) {
		if ( strcasecmp( res.c_str(), "cpus" ) == 0 ||
		     strcasecmp( res.c_str(), "memory" ) == 0 ||
		     strcasecmp( res.c_str(), "disk" ) == 0 ||
		     strcasecmp( res.c_str(), "swap" ) == 0 )
		{
			continue;
		}
		std::string req_str;
		int req_val = 0;
		formatstr( req_str, "%s%s", ATTR_REQUEST_PREFIX, res.c_str() );
		job_ad->LookupInteger( req_str, req_val );
		match_ad->Assign( res, req_val );
    }

	if( result ) {
		// If successful, remove attribute claiming this slot is partitionable
		// and instead mark it as dynamic. This is what the startd does. Plus,
		// removing the paritionable attribute means if we happen to call
		// fixupPartitionableSlot again in the future on this same ad, we 
		// won't mangle it a second time.
		match_ad->Assign(ATTR_SLOT_DYNAMIC, true);
		match_ad->Assign(ATTR_SLOT_PARTITIONABLE,false);
		match_ad->Assign(ATTR_SLOT_TYPE, "Dynamic");
		dprintf(D_FULLDEBUG,
				"Partitionable slot %s adjusted for job %d.%d: "
				"cpus = %ld, memory = %ld, disk = %ld\n",
				slot_name, job_id.cluster, job_id.proc, (long)cpus, (long)memory, (long)disk);
	}

	return result;
}

bool
ScheddNegotiate::sendResourceRequestList(Sock *sock)
{
	m_jobs_can_offer = scheduler_maxJobsToOffer();

	while (m_num_resource_reqs_to_send > 0) {

		// Don't call nextJob() at the start of sending our first set
		// of request ads. If we received match results before this,
		// then nextJob() has already been called to ready the first
		// ad that we want to send. If we haven't received match
		// results, then sendJobInfo() will call nextJob() to ready
		// the first ad.
		if ( m_first_rrl_request ) {
			m_first_rrl_request = false;
		} else {
			nextJob();
		}

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
			m_jobs->pop_front();
		}

		m_num_resource_reqs_sent++;
		m_num_resource_reqs_to_send--;

		extern void IncrementResourceRequestsSent();
		IncrementResourceRequestsSent();
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

	if (IsDebugCategory(D_DYE)) {
		std::string details;
		m_current_job_ad.LookupString(ATTR_OWNER, details);
		details += " "; details += m_owner;
		int count = 0, aid = 0;
		m_current_job_ad.LookupInteger(ATTR_RESOURCE_REQUEST_COUNT, count);
		m_current_job_ad.LookupInteger(ATTR_AUTO_CLUSTER_ID, aid);
		formatstr_cat(details, " %d |%d|%d.%d|", count, aid, m_current_job_id.cluster, m_current_job_id.proc);
		dprintf(D_DYE, "\t%s\n", details.c_str());
	}

		// Tell the negotiator that we understand pslot preemption
	m_current_job_ad.Assign(ATTR_WANT_PSLOT_PREEMPTION, true);
		
		// request match diagnostics
		// 0 = no match diagnostics
		// 1 = match diagnostics string
		// 2 = match diagnostics string decorated w/ autocluster + jobid
		// 3 = (same as 2) + optional diagnostics ad
	int match_diag = 2;
	// TODO: only turn on diag 3 only for jobs that request it?
	if (m_can_do_match_diag_3) { match_diag = 3; }
	m_current_job_ad.Assign(ATTR_WANT_MATCH_DIAGNOSTICS, (int) match_diag);

		// Send the ad to the negotiator
	int putad_result = 0;
	std::string auto_cluster_attrs;
	if ( just_sig_attrs &&
		 m_current_job_ad.LookupString(ATTR_AUTO_CLUSTER_ATTRS, auto_cluster_attrs) )
	{
		// don't send the entire job ad; just send significant attrs
		classad::References sig_attrs;

		StringTokenIterator list(auto_cluster_attrs);
		const std::string *attr = nullptr;
		while ((attr = list.next_string())) { sig_attrs.insert(*attr); }

		// besides significant attrs, we also always want to send these attrs cuz
		// the matchmaker explicitly looks for them (for dprintfs or whatever).
		sig_attrs.insert(ATTR_OWNER);
		sig_attrs.insert(ATTR_CLUSTER_ID);
		sig_attrs.insert(ATTR_PROC_ID);
		sig_attrs.insert(ATTR_RESOURCE_REQUEST_CONSTRAINT);
		sig_attrs.insert(ATTR_RESOURCE_REQUEST_COUNT);
		sig_attrs.insert(ATTR_GLOBAL_JOB_ID);
		sig_attrs.insert(ATTR_AUTO_CLUSTER_ID);
		sig_attrs.insert(ATTR_WANT_MATCH_DIAGNOSTICS);
		sig_attrs.insert(ATTR_WANT_PSLOT_PREEMPTION);

		if (IsDebugVerbose(D_MATCH)) {
			std::string tmp;
			sPrintAdAttrs(tmp, m_current_job_ad, sig_attrs);
			dprintf(D_MATCH | D_VERBOSE, "resource request for job %d.%d :\n%s\n", m_current_job_id.cluster, m_current_job_id.proc, tmp.c_str());
		}

		// ship it!
		putad_result = putClassAd(sock, m_current_job_ad, 0, &sig_attrs);
	} else {
		// send the entire classad.  perhaps we are doing this because the
		// ad does not have ATTR_AUTO_CLUSTER_ATTRS defined for some reason,
		// or perhaps we are doing this because we were explicitly told to do so.

		if (IsDebugVerbose(D_MATCH)) {
			std::string tmp;
			sPrintAd(tmp, m_current_job_ad);
			dprintf(D_MATCH | D_VERBOSE, "resource request (all) for job %d.%d :\n%s\n", m_current_job_id.cluster, m_current_job_id.proc, tmp.c_str());
		}

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

	if (IsDebugCategory(D_DYE)) {
		std::string details;
		switch(m_operation) {
		case REJECTED_WITH_REASON: details = m_reject_reason; break;
		case SEND_JOB_INFO: {
			if ( m_jobs && ! m_jobs->empty()) { auto & rr = m_jobs->front(); formatstr(details, "ac=%d", rr.getAutoClusterId()); }
			} break;
		case PERMISSION_AND_AD: {
			m_match_ad.LookupString(ATTR_NAME,details);
			JOB_ID_KEY jid;
			m_match_ad.LookupInteger(ATTR_RESOURCE_REQUEST_CLUSTER, jid.cluster);
			m_match_ad.LookupInteger(ATTR_RESOURCE_REQUEST_PROC, jid.proc);
			formatstr_cat(details, " %d ", m_current_resources_delivered);
			// TODO: have negotiator send RR id back, not just job id.
			details += "|?|"; details += (std::string)(jid); details += "|";
			} break;
		case END_NEGOTIATE: {
			if (RRLRequestIsPending()) { details = "<prefetch>"; } else { details = "<end>"; }
			} break;
		}
		dprintf(D_DYE, "%08x_%06d:%s\t%s\t%s\n", sock->getUniqueId(), m_instance,
			m_owner.c_str(), getCommandStringSafe(m_operation), details.c_str());
	}

	switch( m_operation ) {

	case REJECTED:
		m_reject_reason = "Unknown reason";
		// Fall through...
		//@fallthrough@
	case REJECTED_WITH_REASON: {
		// To support resource request lists, the
		// reject reason may end with "...|autocluster|cluster.proc|"
		// if so, reset m_current_auto_cluster_id and m_current_job_id
		// with the values contained in the reject reason, and truncate
		// this information out of m_reject_reason.
		size_t pos = m_reject_reason.find('|');
		if ( pos != std::string::npos ) {
			StringTokenIterator tok(m_reject_reason, "|");
			/*const char *reason =*/ tok.next();
			const char *ac = tok.next();
			if (ac) {
				m_current_auto_cluster_id = atoi(ac);
			}
			const char *jobid = tok.next();
			if (jobid) {
				int rr_cluster = 0, rr_proc = 0;
				StrIsProcId(jobid,rr_cluster,rr_proc,nullptr);
				if (rr_cluster != m_current_job_id.cluster || rr_proc != m_current_job_id.proc) {
					m_current_resources_delivered = 0;
				}
				m_current_job_id.cluster = rr_cluster;
				m_current_job_id.proc = rr_proc;
			}
			m_reject_reason.erase(pos); // will truncate string at pos
			trim(m_reject_reason);
		}
		// did we get a rejection ad?
		if (m_reject_ad && m_reject_ad->size() > 0) {
			if (IsDebugVerbose(D_MATCH)) {
				std::string adbuf;
				dprintf(D_MATCH | D_VERBOSE, "Got a match rejection ad from %s:\n%s",
					m_negotiator_name.c_str(), formatAd(adbuf, *m_reject_ad, "\t"));
			}
		}
		scheduler_handleJobRejected( m_current_job_id, m_current_auto_cluster_id, m_reject_reason.c_str() );
		m_jobs_rejected++;
		setAutoClusterRejected( m_current_auto_cluster_id );
		nextJob();
		break;
	}

	case SEND_JOB_INFO:
		//dprintf(D_MATCH, "got SEND_JOB_INFO\n");
		m_num_resource_reqs_sent = 0;  // clear counter of reqs sent this round
		notSendingResourceRequests();
		if( !sendJobInfo(sock) ) {
				// We failed to talk to the negotiator, so close the socket.
			return MESSAGE_FINISHED;
		}
		break;

	case SEND_RESOURCE_REQUEST_LIST:
		//dprintf(D_MATCH, "got SEND_RESOURCE_REQUEST_LIST\n");
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

		bool offline = false;
		m_match_ad.LookupBool(ATTR_OFFLINE,offline);

		if( offline ) {
			dprintf(D_ALWAYS,"Job %d.%d (delivered=%d) matched to offline machine %s.\n",
					m_current_job_id.cluster,m_current_job_id.proc,m_current_resources_delivered,slot_name);
			nextJob();
			break;
		}

		if( scheduler_handleMatch(m_current_job_id,m_claim_id.c_str(),m_extra_claims.c_str(), m_match_ad,slot_name) )
		{
			m_jobs_matched++;
		}

		nextJob();

		break;
	}

	case END_NEGOTIATE:
		if (RRLRequestIsPending()) {
			dprintf(D_ALWAYS, "Finished sending rrls to negotiator\n");
		} else {
			dprintf( D_ALWAYS, "Negotiation ended: %d jobs matched\n",
					 m_jobs_matched );
		}

		m_negotiation_finished = true;
		break;

	default:
		EXCEPT("should never get here (negotiation op %d)",m_operation);

	} // end of switch on m_operation

	if( m_negotiation_finished ) {
			// the following function takes ownership of sock
		scheduler_handleNegotiationFinished( sock );
		sock = nullptr;
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
		if (m_reject_ad) { m_reject_ad->Clear(); } // in case we don't get a rejection ad
		if( !sock->code(m_reject_reason) ) {
			dprintf( D_ALWAYS,
					 "Can't receive reject reason from negotiator\n" );
			return false;
		}
		// a 24.X negotiator may also return a match diagnostics ad
		if (m_can_do_match_diag_3 && ! sock->peek_end_of_message()) {
			if ( ! m_reject_ad) { m_reject_ad = new ClassAd(); }
			if ( ! getClassAd(sock, *m_reject_ad)) {
				dprintf( D_ALWAYS, "Can't get my reject ad from negotiator\n" );
				return false;
			}
		}
		break;

	case REJECTED:
		if (m_reject_ad) { m_reject_ad->Clear(); } // in case we don't get a rejection ad
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
		char *claim_id = nullptr;
		if( !sock->get_secret(claim_id) || !claim_id ) {
			dprintf( D_ALWAYS,
					 "Can't receive ClaimId from negotiator\n" );
			return false;
		}
		m_claim_id = claim_id;
		free( claim_id );

		size_t space = m_claim_id.find(' ');
		if (space != std::string::npos) {
			m_extra_claims = m_claim_id.substr(space + 1, std::string::npos);
			m_claim_id = m_claim_id.substr(0, space);
		} else {
			m_extra_claims.clear();
		}

		m_match_ad.Clear();

			// get startd ad from negotiator
		if( !getClassAd(sock, m_match_ad) ) {
			dprintf( D_ALWAYS,
					 "Can't get my match ad from negotiator\n" );
			return false;
		}
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
