/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "startd.h"
#include "basename.h"
#include "hook_utils.h"


// // // // // // // // // // // //
// StartdHookMgr
// // // // // // // // // // // //

StartdHookMgr::StartdHookMgr()
	: HookClientMgr(),
	  NUM_HOOKS(3),
	  UNDEFINED((char*)1),
	  m_keyword_hook_paths(hashFunction)
{
	dprintf( D_FULLDEBUG, "Instantiating a StartdHookMgr\n" );
}


StartdHookMgr::~StartdHookMgr()
{
	dprintf( D_FULLDEBUG, "Deleting the StartdHookMgr\n" );

		// Delete our copies of the paths for each hook.
	clearHookPaths();

}


void
StartdHookMgr::clearHookPaths()
{
	int i;
	std::string key;
	char** hook_paths;
	m_keyword_hook_paths.startIterations();
	while (m_keyword_hook_paths.iterate(key, hook_paths)) {
		for (i=0; i<NUM_HOOKS; i++) {
			if (hook_paths[i] && hook_paths[i] != UNDEFINED) {
				free(hook_paths[i]);
			}
		}
		delete [] hook_paths;
	}
	m_keyword_hook_paths.clear();
}


bool
StartdHookMgr::initialize()
{
	reconfig();
	return HookClientMgr::initialize();
}


bool
StartdHookMgr::reconfig()
{
		// Clear out our old copies of each hook's path.
	clearHookPaths();

	return true;
}


char*
StartdHookMgr::getHookPath(HookType hook_type, Resource* rip)
{
	char* keyword = rip->getHookKeyword();

	if (!keyword) {
			// Nothing defined, bail now.
		return NULL;
	}

	int i;
	std::string key(keyword);
	char** hook_paths;
	if (m_keyword_hook_paths.lookup(key, hook_paths) < 0) {
			// No entry, initialize it.
		hook_paths = new char*[NUM_HOOKS];
		for (i=0; i<NUM_HOOKS; i++) {
			hook_paths[i] = NULL;
		}
		m_keyword_hook_paths.insert(key, hook_paths);
	}

	char* path = hook_paths[(int)hook_type];
	if (!path) {
		std::string _param;
		formatstr(_param, "%s_HOOK_%s", keyword, getHookTypeString(hook_type));
		bool hperr = !validateHookPath(_param.c_str(), path);
        // Here the distinction between undefined hook and a hook path error 
        // is being collapsed
		if ((path == NULL) || (hperr)) {
			hook_paths[(int)hook_type] = UNDEFINED;
		}
		else {
			hook_paths[(int)hook_type] = path;
		}
		rip->dprintf(D_FULLDEBUG, "Hook %s: %s\n", _param.c_str(),
					 path ? path : "UNDEFINED");
	}
	else if (path == UNDEFINED) {
		path = NULL;
	}
	return path;
}


FetchClient*
StartdHookMgr::buildFetchClient(Resource* rip)
{
	char* hook_path = getHookPath(HOOK_FETCH_WORK, rip);
	if (!hook_path) {
			// No fetch hook defined for this slot, abort.
		return NULL;
	}
	FetchClient* new_client = new FetchClient(rip, hook_path);
	return new_client;
}


bool
StartdHookMgr::invokeHookFetchWork(Resource* rip)
{
	FetchClient* fetch_client = buildFetchClient(rip);
	if (!fetch_client) {
		return false;
	}
	return fetch_client->startFetch();
}


bool
StartdHookMgr::handleHookFetchWork(FetchClient* fetch_client)
{
	ClassAd* job_ad = NULL;
	Resource* rip = fetch_client->m_rip;
	float rank = 0;
		// Are we currently in Claimed/Idle with a fetched claim?
	bool idle_fetch_claim = (rip->r_cur->type() == CLAIM_FETCH
							 && rip->state() == claimed_state
							 && rip->activity() == idle_act);

	if (!(job_ad = fetch_client->reply())) {
			// No work or error reading the reply, bail out.
			// Try other hooks?
		if (idle_fetch_claim) {
				// we're currently Claimed/Idle with a fetched
				// claim. If the fetch hook just returned no data, it
				// means we're out of work, we should evict this
				// claim, and return to the Owner state.
			rip->terminateFetchedWork();
		}
		return false;
	}
	
	bool willing = true;
	if(rip->state() == preempting_state) {
		// There are race bugs if we try to start a new job while waiting
		// for the old starter to exit.  So don't do that. #3076 
		willing = false;
		rip->dprintf(D_FULLDEBUG, "Rejecting new fetchwork; still preempting the previous job.\n");
	}
	else if (!rip->willingToRun(job_ad)) {
		// If we got here, we've got a ClassAd describing the job, so
		// see if this slot is willing to run it.
		willing = false;
	}
	else {
		rank = rip->compute_rank(job_ad);
		rip->dprintf(D_FULLDEBUG, "Rank of this fetched claim is: %f\n", rank);
		if (rip->state() == claimed_state && !idle_fetch_claim) {
				// Make sure it's got a high enough rank to preempt us.
			if (rank <= rip->r_cur->rank()) {
					// For fetched jobs, there's no user priority
					// preemption, so the newer claim has to have higher,
					// not just equal rank.
				rip->dprintf(D_ALWAYS, "Fetched claim doesn't have sufficient rank, refusing.\n");
				willing = false;
			}
		}
	}

		// Make sure that the job classad defines ATTR_HOOK_KEYWORD,
		// and if not, insert this slot's keyword.
	if (!job_ad->LookupString(ATTR_HOOK_KEYWORD, NULL, 0)) {
		char* keyword = rip->getHookKeyword();
		ASSERT(keyword && keyword != UNDEFINED);
		job_ad->Assign(ATTR_HOOK_KEYWORD, keyword);
	}

		// No matter what, if the reply fetch hook is configured, invoke it.
	hookReplyFetch(willing, job_ad, rip);

	if (willing) {
		Claim* leftover_claim = NULL;
		if (rip->can_create_dslot()) {
			Resource * new_rip = create_dslot(rip, job_ad, leftover_claim);
			if (new_rip) { rip = new_rip; }
			else { willing = false; }
		}
	}


	if (!willing) {
			// TODO-fetch: matchmaking on other slots?
		if (idle_fetch_claim) {
				// The slot is Claimed/Idle with a fetch claim. If we
				// just fetched work and aren't willing to run it, we
				// need to evict this claim and return to Owner.
			rip->terminateFetchedWork();
		}
		return false;
	}

		// We're ready to start running the job, so we need to update
		// the current Claim and Client objects to remember this work.
	rip->createOrUpdateFetchClaim(job_ad, rank);

		// Once we've done that, the Claim object in the Resource has
		// control over the job classad, so we want to NULL-out our
		// copy here to avoid a double-free.
	fetch_client->clearReplyAd();

		// Now, depending on our current state, initiate a state change.
	if (rip->state() == claimed_state) {
		if (idle_fetch_claim) {
				// We've got an idle fetch claim and we just got more
				// work, so we should spawn it.
			rip->spawnFetchedWork();
			return true;
		}
			// We're already claimed, but not via an idle fetch claim,
			// so we need to preempt the current job first.
		rip->dprintf(D_ALWAYS, "State change: preempting claim based on "
					 "machine rank of fetched work.\n");

			// Force resource to take note of the preempting claim.
			// This results in a reversible transition to the
			// retiring activity.  If the preempting claim goes
			// away before the current claim retires, the current
			// claim can unretire and continue without any disturbance.
		rip->eval_state();
	}
	else {
			// Start moving towards Claimed so we actually spawn the job.
		dprintf(D_ALWAYS, "State change: Finished fetching work successfully\n");
		rip->r_state->set_destination(claimed_state);
	}
	return true;
}

void
StartdHookMgr::hookReplyFetch(bool accepted, ClassAd* job_ad, Resource* rip)
{
	char* hook_path = getHookPath(HOOK_REPLY_FETCH, rip);
	if (!hook_path) {
		return;
	}

		// Since we're not saving the output, this can just live on
		// the stack and be destroyed as soon as we return.
	HookClient hook_client(HOOK_REPLY_FETCH, hook_path, false);

	//PRAGMA_REMIND("tj, flatten SlotEval here?")

		// Construct the output to write to STDIN.
	std::string hook_stdin;
	sPrintAd(hook_stdin, *job_ad);
	hook_stdin += "-----\n";  // TODO-fetch: better delimiter?
	sPrintAd(hook_stdin, *rip->r_classad);
	if (accepted) {
			// Also include the claim id in the slot classad.
		hook_stdin += ATTR_CLAIM_ID;
		hook_stdin += " = \"";
		hook_stdin += rip->r_cur->id();
		hook_stdin += "\"\n";
	}

	ArgList args;
	args.AppendArg((accepted ? "accept" : "reject"));

	spawn(&hook_client, &args, hook_stdin);
}


void
StartdHookMgr::hookEvictClaim(Resource* rip)
{
	char* hook_path = getHookPath(HOOK_EVICT_CLAIM, rip);
	if (!hook_path) {
		return;
	}

		// Since we're not saving the output, this can just live on
		// the stack and be destroyed as soon as we return.
	HookClient hook_client(HOOK_EVICT_CLAIM, hook_path, false);

	//PRAGMA_REMIND("tj, flatten SlotEval here?")

		// Construct the output to write to STDIN.
	std::string hook_stdin;
	sPrintAd(hook_stdin, *rip->r_cur->ad());
	hook_stdin += "-----\n";  // TODO-fetch: better delimiter?
	sPrintAd(hook_stdin, *rip->r_classad);
		// Also include the claim id in the slot classad.
	hook_stdin += ATTR_CLAIM_ID;
	hook_stdin += " = \"";
	hook_stdin += rip->r_cur->id();
	hook_stdin += "\"\n";

	spawn(&hook_client, NULL, hook_stdin);
}


// // // // // // // // // // // //
// FetchClient class
// // // // // // // // // // // //

FetchClient::FetchClient(Resource* rip, const char* hook_path)
	: HookClient(HOOK_FETCH_WORK, hook_path, true)
{
	m_rip = rip;
	m_job_ad = NULL;
}


FetchClient::~FetchClient()
{
	if (m_job_ad) {
		delete m_job_ad;
		m_job_ad = NULL;
	}
}


bool
FetchClient::startFetch()
{
	ASSERT(m_rip);
	ClassAd slot_ad;

	m_rip->publish_single_slot_ad(slot_ad, time(NULL), Resource::Purpose::for_workfetch);

	std::string slot_ad_txt;
	sPrintAd(slot_ad_txt, slot_ad);
	resmgr->m_hook_mgr->spawn(this, NULL, slot_ad_txt);
	m_rip->startedFetch();
	return true;
}


ClassAd*
FetchClient::reply()
{
	return m_job_ad;
}


void
FetchClient::hookExited(int exit_status) {
	HookClient::hookExited(exit_status);
	if (m_std_err.length()) {
		dprintf(D_ALWAYS,
				"Warning, hook %s (pid %d) printed to stderr: %s\n",
				m_hook_path, (int)m_pid, m_std_err.c_str());
	}
	if (m_std_out.length()) {
		ASSERT(m_job_ad == NULL);
		m_job_ad = new ClassAd();
		MyStringTokener tok;
		tok.Tokenize(m_std_out.c_str());
		const char* hook_line = NULL;
		while ((hook_line = tok.GetNextToken("\n", true))) {
			if (!m_job_ad->Insert(hook_line)) {
				dprintf(D_ALWAYS, "Failed to insert \"%s\" into ClassAd, "
						"ignoring invalid hook output\n", hook_line);
					// TODO-pipe howto abort?
					// Tell the slot this fetch invocation completed.
				m_rip->fetchCompleted();
				return;
			}
		}
	}
	else {
		dprintf(D_FULLDEBUG, "Hook %s (pid %d) returned no data\n",
				m_hook_path, (int)m_pid);
	}
		// Let the work manager know this fetch result is done.
	resmgr->m_hook_mgr->handleHookFetchWork(this);

		// Tell the slot this fetch invocation completed.
	m_rip->fetchCompleted();
}


void
FetchClient::clearReplyAd(void) {
	m_job_ad = NULL;
}
