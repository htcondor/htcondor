/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef _CONDOR_STARTD_HOOK_MGR_H
#define _CONDOR_STARTD_HOOK_MGR_H

#include "condor_common.h"
#include "startd.h"
#include "HookClientMgr.h"
#include "HookClient.h"

class FetchClient;

/**
   The StartdHookMgr manages all attempts to fetch work via hooks.
*/

class StartdHookMgr : public HookClientMgr
{
public:
	StartdHookMgr(); 
	~StartdHookMgr();

	bool initialize();
	bool reconfig();

		/**
		   Invoke the fetching hook with a slot classad, and register a
		   handler for the reply.

		   This never initiates a state change, only the handler does.

		   @param rip Pointer to a Resource object to try to fetch work for.
		   @return True if a request was sent and a handler registered.
		*/
	bool invokeHookFetchWork(Resource* rip);

		/**
		   Handle a reply to a request to fetch work.
		   
		   2 Possible replies:
		   #1 Indication there's no work to do:
		   ?? See if another server might have work and try another fetch.
		   -- Return false.
		   #2 ClassAd describing the job to run:
		   -- Verify the slot is still available or return false.
           ?? If not, do matchmaking on all other slots to find a fit?
		   -- Create or update the current Claim and Client objects.
		   -- Initiate a state change to Claimed/Busy.
		   -- Return true.

		   @param fetch_client Pointer to the FetchClient that replied.

		   @return True if work was accepted.
		*/
	bool handleHookFetchWork(FetchClient* fetch_client);

		/**
		   Invoke the REPLY_CLAIM hook to tell the outside world what
		   we decided to do with a given claim request.
		   @param claimed Did we accept the claim or not?
		   @param job_ad ClassAd of the job we considered.
		   @param rip Resource that considered the claim.
		 */
	void hookReplyClaim(bool claimed, ClassAd* job_ad, Resource* rip);

		/**
		   Invoke the EVICT_CLAIM hook to tell the outside world that
		   the fetched claim on a given slot is being evicted.
		   The job ClassAd written to stdin is of the most recent job
		   we fetched.
		   @param rip Resource that is evicting its fetched claim.
		 */
	void hookEvictClaim(Resource* rip);

private:
		/// Number of hooks used by the startd
	const int NUM_HOOKS;

		/**
		   Magic "constant" char* that we use to remember that we
		   tried to lookup and validate a given hook path or keyword,
		   since we only do lazy discovery and NULL means we haven't
		   even tried to find it yet.
		*/
	char* UNDEFINED;

	FetchClient* buildFetchClient(Resource* rip);

		/**
		   Find the keyword to use for the given resource/slot.
		   @param rip Resource you want the keyword for.
		   @return Hook keyword to use, or NULL if none.
		*/
	char* getHookKeyword(Resource* rip);

		/**
		   Returns the valid path for the given hook on a specific slot.
		   @param hook_type What kind of hook we need the path for.
		   @param rip Resource you need the path for.
		   @return Path to the valid hook, or NULL if undefined or invalid.
		*/
	char* getHookPath(HookType hook_type, Resource* rip);

		/// Clears out all the hook paths we've validated and saved.
	void clearHookPaths( void );

		/**
		   Array of hook keywords for each slot.  Indexed by slot id.
		   If uninitialized, the value is NULL.  If we already
		   searched for the keyword and it's not defined in the config
		   file, the value is the 'UNDEFINED' constant declared above.
		*/
	ExtArray<char*> m_slot_hook_keywords;

		/**
		   HashTable that contains an array of strings for each
		   keyword. Table is hashed on hook keyword.  Each array is
		   indexed by the HookType enum, and stores the validated path
		   of the implementation for each hook, UNDEFINED if we
		   searched and it's not defined or invalid, and NULL if it
		   hasn't been initialized yet.
		*/
	HashTable<MyString, char**> m_keyword_hook_paths;

		/// Default hook keyword if the per-slot settings aren't defined.
	char* m_startd_job_hook_keyword;

};


/**
   Each FetchClient object manages an invocation of the fetch work hook.
*/
class FetchClient : public HookClient
{
public:
	friend class StartdHookMgr;

	FetchClient(Resource* rip, const char* hook_path);
	virtual ~FetchClient();
	virtual void hookExited(int exit_status);

	bool startFetch();
	ClassAd* reply();

		/**
		   Clear out the ClassAd pointer once it's been passed on to
		   another entity to control the memory.  Once we create a
		   Claim object for work we've fetched, the Claim has full
		   control over the ClassAd, so we need to clear out our copy
		   of the pointer to ensure we don't try to delete it twice.
		*/
	void clearReplyAd();

protected:
	Resource* m_rip;
	ClassAd* m_job_ad;
};


#endif /* _CONDOR_STARTD_HOOK_MGR_H */
