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
 * Represents a set of jobs sharing the same priority and auto cluster id.
 *
 * The auto cluster ids are only assumed to be internally consistent
 * at the time the snapshot of the job queue was taken.  They are not used
 * to reference any external data structure.
 */
class ResourceRequestCluster {
 public:
	ResourceRequestCluster(int auto_cluster_id);

		// appends a job to this cluster
	void addJob(PROC_ID job_id);

		// returns true if a job was found; o.w. false
	bool popJob(PROC_ID &job_id);

		// returns number of jobs in this cluster
	size_t size() { return m_job_ids.size(); }

		// returns the auto cluster id for this cluster
	int getAutoClusterId() const { return m_auto_cluster_id; }
 private:

	int m_auto_cluster_id;
	std::list<PROC_ID> m_job_ids;
};

/*
 * ResourceRequestList
 *
 * Contains an ordered list of job auto clusters for matchmaking.  All
 * jobs in the list are owned by the same user (or accounting group).
 * 
 * The ResourceRequestClusters in this list are deleted when the list
 * is deleted.
 */
class ResourceRequestList: public std::list<ResourceRequestCluster *> {
 public:
	~ResourceRequestList();
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

		// Begins asynchronously processing negotiation operations
		// sent by the negotiator.  Assumes that the initial
		// negotiation header has already been read (the owner,
		// significant attributes, etc).
	void negotiate(Sock *sock);

		// returns the job owner/user name (or accounting group) we are serving
	char const *getMatchUser();

		// returns name of remote pool or NULL if none
	char const *getRemotePool();

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
	virtual void scheduler_handleJobRejected(PROC_ID job_id,char const *reason) = 0;

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
	ResourceRequestList *m_jobs;
		// how many resources are we requesting with this request?
	int m_current_resources_requested;
		// how many resources have been delivered so far with this request?
	int m_current_resources_delivered;
		// how many more resources can we offer to the matchmaker?
		// If -1, then we don't limit the offered resources.
	int m_jobs_can_offer;

 private:
	std::set<int> m_rejected_auto_clusters;

	std::string m_owner;
	std::string m_remote_pool;

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

	virtual void scheduler_handleJobRejected(PROC_ID job_id,char const *reason);

	virtual bool scheduler_handleMatch(PROC_ID job_id,char const *claim_id, char const *extra_claims, ClassAd &match_ad, char const *slot_name);

	virtual void scheduler_handleNegotiationFinished( Sock *sock );

};

#endif
