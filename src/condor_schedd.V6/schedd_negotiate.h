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

#ifndef DC_SCHEDD_NEGOTIATE_H
#define DC_SCHEDD_NEGOTIATE_H

#include "dc_message.h"
#include "prio_rec.h"

 /*
 * ResourceRequestCluster
 *
 * Represents a set of jobs sharing the same submitter, priority and auto cluster id.
 * 
 * the autocluster_id may not be an actual autocluster id, but it should be different
 * for each entry in the ResourceRequestList. Think of it as the resource request id.
 *
 */
class ResourceRequestCluster {
public:
	ResourceRequestCluster(int id=0, int capacity=0) : m_autocluster_id(id) {
		if (capacity) m_jobs.reserve(capacity);
	}
	void addJob(PROC_ID jid) { m_jobs.push_back(jid); }

	// returns number of jobs in this cluster
	size_t size() { return m_jobs.size(); }

	// returns the auto cluster id for this cluster
	int getAutoClusterId() const { return m_autocluster_id; }

	using container = std::vector<PROC_ID>;
	using iterator = container::iterator;
	using const_iterator = container::const_iterator;
	iterator begin() { return m_jobs.begin(); }
	const_iterator begin() const { return m_jobs.cbegin(); }
	iterator end() { return m_jobs.end(); }
	const_iterator end() const { return m_jobs.cend(); }

protected:
	std::vector<PROC_ID> m_jobs;
	int                  m_autocluster_id{0};
};

/*
* ResourceRequestList
*
* Contains an ordered list of job auto clusters for matchmaking.  All
* jobs in the list are owned by the same user (or accounting group).
* 
* The list is owned by an instance of ScheddNegotiate and consumed (deleted) by it
* as the conversation proceeds. Currently ResourceRequestClusters in this list
* and are also deleted during negotiation, but planned future changes will
* Make the resource request clusters persist, but list will not.
*/
class ResourceRequestList {
public:
	void add(int id, PROC_ID & jid) {
		if ( ! items.empty() && items.back().getAutoClusterId() == id) {
			items.back().addJob(jid);
		} else {
			items.emplace_back(id).addJob(jid);
		}
		largest_cluster = MAX(largest_cluster, items.back().size());
	}
	int currentId() {
		if (items.empty()) return -1;
		return items.back().getAutoClusterId();
	}
	bool empty() { return items.empty(); }
	size_t size() { return items.size(); }
	ResourceRequestCluster & front() { return items.front(); }
	void pop_front() { items.pop_front(); }
	void flatten();

protected:
	std::deque<ResourceRequestCluster> items;
	size_t largest_cluster{0};
};


/*
 * ScheddNegotiate
 *
 * This class handles the schedd-side of the negotiation loop.  It is
 * derived from DCMsg in order to take advantage of its asynchronous
 * message passing framework.  Essentially, there is a non-blocking read
 * before each negotiation operation that we receive from the negotiator.
 * This allows the schedd to do other things while the negotiator is
 * busy finding a match for the job.
 *
 * Rather than directly calling functions of the schedd, virtual methods
 * are used for this purpose, making this class reusable for both the
 * main schedd, the dedicated schedd, and possibly other cases.
 *
 */

class JobQueueJob; // forward ref

class ScheddNegotiate: public DCMsg {
 public:
	ScheddNegotiate(
		int cmd,
		ResourceRequestList *jobs,
		char const *owner,
		char const *remote_pool
	);

	virtual ~ScheddNegotiate();

	void setWillMatchClaimedPslots(bool will_match) { m_will_match_claimed_pslots = will_match; }
	void setMatchCaps(std::string_view caps);
	void setNegotiatorName(std::string_view name) { m_negotiator_name = name; }

		// Begins asynchronously processing negotiation operations
		// sent by the negotiator.  Assumes that the initial
		// negotiation header has already been read (the owner,
		// significant attributes, etc).
	void negotiate(Sock *sock);

		// returns the job owner/user name (or accounting group) we are serving
	char const *getMatchUser();

		// returns name of remote pool or NULL if none
	char const *getRemotePool();

		// returns self-reported name of negotiator (only 24.x negotiators report a name)
		// (for use when a pool has multiple negotiators)
	char const *getNegotiatorName();

	int getNumJobsMatched() const { return m_jobs_matched; }

	int getNumJobsRejected() const { return m_jobs_rejected; }

		// Returns true if we got everything we wanted from the negotiator
		// and false if "I can't get no ..."
	bool getSatisfaction();

		// Convert a patitionable slot into a dynamic slot. Hopefully this
		// method does this the same way the startd does. 
	static bool fixupPartitionableSlot(ClassAd *job_ad, ClassAd *match_ad);

		///////////// virtual functions for scheduler to define  //////////////

		// Returns false if job does not exist.  Otherwise, job_ad is
		// made to be a copy of the requested job.
	virtual bool scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad ) = 0;

		// returns false if we should still try getting a match
	virtual bool scheduler_skipJob(JobQueueJob * job, ClassAd *match_ad, bool &skip_all, const char * &skip_because) = 0;

		// returns false if we should skip this request ad (i.e. and not send it to the negotiator at all)
		// if return is true, and match_max is not null, match_max will be set to the maximum matches constraint
		// and the request constraint expression will be added to the request_ad
	virtual bool scheduler_getRequestConstraints(PROC_ID job_id, ClassAd &request_ad, int * match_max) = 0;

		// a job was rejected by the negotiator
	virtual void scheduler_handleJobRejected(PROC_ID job_id,int autocluster_id, char const *reason) = 0;

		// returns true if the match was successfully handled (so far)
	virtual bool scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name) = 0;

	virtual void scheduler_handleNegotiationFinished( Sock *sock ) = 0;

		// Return the number of resource requests we should offer in this negotiation round.
		// -1 indicates there is no limit.
		// This is useful in helping to enforce MAX_JOBS_RUNNING as a single resource request
		// can bring back thousands of matches from the negotiator.
	virtual int scheduler_maxJobsToOffer() {return -1;};

		///////// end of virtual functions for scheduler to define  //////////

		// We've sent a RRL, but haven't heard back with a match yet
	bool RRLRequestIsPending() const { return ((getNumJobsMatched() == 0) && (getNumJobsRejected() == 0));} 

 protected:
	ResourceRequestList *m_jobs{nullptr};
	int m_instance{0}; // instance id used for logging
		// how many resources are we requesting with this request?
	int m_current_resources_requested{1};
		// how many resources have been delivered so far with this request?
	int m_current_resources_delivered{0};
		// how many more resources can we offer to the matchmaker?
		// If -1, then we don't limit the offered resources.
	int m_jobs_can_offer{-1};
		// will matchmaker match pslots that we have claimed?
	bool m_will_match_claimed_pslots{false};
	bool m_can_do_match_diag_3{false};
	bool m_can_do_match_dye{false};
	ClassAd * m_reject_ad{nullptr};   // detailed match rejection ad (24.x negotiators or later)

 private:
	std::set<int> m_rejected_auto_clusters;

	std::string m_owner;
	std::string m_remote_pool;
	std::string m_negotiator_name;

	int m_current_auto_cluster_id;
	PROC_ID m_current_job_id;
	ClassAd m_current_job_ad;

	int m_jobs_rejected;
	int m_jobs_matched;

	int m_num_resource_reqs_sent; // used when sending a resource request list
	int m_num_resource_reqs_to_send; // used when sending a resource request list

	bool m_negotiation_finished;
	bool m_first_rrl_request;

		// data in message received from negotiator
	int m_operation;             // the negotiation operation
	std::string m_reject_reason; // why the job was rejected
	std::string m_claim_id;      // the string "null" if none
	std::string m_extra_claims;

	ClassAd m_match_ad;          // the machine we matched to

		// Updates m_current_job_id to next job in the list
		// returns false if no more jobs
	bool nextJob();

		// returns true if the specified cluster was rejected
	bool getAutoClusterRejected(int auto_cluster_id);

		// marks the specified cluster as rejected
	void setAutoClusterRejected(int auto_cluster_id);

	bool sendJobInfo(Sock *sock, bool just_sig_attrs=false);

		// negotiator sent SEND_JOB_INFO, so it doesn't want to hear about resource requests lists
	void notSendingResourceRequests();

	bool sendResourceRequestList(Sock *sock);

		/////////////// DCMsg hooks ///////////////

	virtual bool readMsg( DCMessenger *messenger, Sock *sock );

	virtual bool writeMsg( DCMessenger *messenger, Sock *sock );

	virtual MessageClosureEnum messageReceived( DCMessenger *, Sock *);

		/////////// End of DCMsg hooks ////////////
};

/* DedicatedScheddNegotiate is a class that overrides virtual methods
   called by ScheddNegotiate when it requires actions to be taken by
   the schedd during negotiation.  See the definition of
   ScheddNegotiate for a description of these functions.
*/
class DedicatedScheddNegotiate: public ScheddNegotiate {
public:
	DedicatedScheddNegotiate(
		int cmd,
		ResourceRequestList *jobs,
		char const *owner,
		char const *remote_pool
	): ScheddNegotiate(cmd,jobs,owner,remote_pool) {}

		// Define the virtual functions required by ScheddNegotiate //

	virtual bool scheduler_getJobAd( PROC_ID job_id, ClassAd &job_ad );

	virtual bool scheduler_skipJob(JobQueueJob * job, ClassAd *match_ad, bool &skip_all, const char * &skip_because); // match ad may be null

	virtual bool scheduler_getRequestConstraints(PROC_ID job_id, ClassAd &request_ad, int * match_max);

	virtual void scheduler_handleJobRejected(PROC_ID job_id,int autocluster_id,char const *reason);

	virtual bool scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name);

	virtual void scheduler_handleNegotiationFinished( Sock *sock );

};

#endif
