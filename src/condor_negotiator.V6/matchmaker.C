/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include <math.h>
#include <float.h>
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"
#include "condor_query.h"
#include "daemon.h"
#include "dc_startd.h"
#include "daemon_types.h"
#include "dc_collector.h"
#include "condor_string.h"  // for strlwr() and friends

// the comparison function must be declared before the declaration of the
// matchmaker class in order to preserve its static-ness.  (otherwise, it
// is forced to be extern.)

static int comparisonFunction (AttrList *, AttrList *, void *);
#include "matchmaker.h"

// possible outcomes of negotiating with a schedd
enum { MM_ERROR, MM_DONE, MM_RESUME };

// possible outcomes of a matchmaking attempt
enum { _MM_ERROR, MM_NO_MATCH, MM_GOOD_MATCH, MM_BAD_MATCH };

typedef int (*lessThanFunc)(AttrList*, AttrList*, void*);

static bool want_simple_matching = false;

Matchmaker::
Matchmaker ()
{
	char buf[64];

	AccountantHost  = NULL;
	PreemptionReq = NULL;
	PreemptionRank = NULL;
	NegotiatorPreJobRank = NULL;
	NegotiatorPostJobRank = NULL;
	sockCache = NULL;

	sprintf (buf, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondStd);

	sprintf (buf, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondPrioPreempt);

	negotiation_timerID = -1;
	GotRescheduleCmd=false;
	
	stashedAds = new AdHash(1000, HashFunc);

	Collectors = NULL;
	
	MatchList = NULL;
	cachedAutoCluster = -1;
	cachedName = NULL;
	cachedAddr = NULL;

	want_simple_matching = false;
	want_matchlist_caching = false;
	ConsiderPreemption = true;

	completedLastCycleTime = (time_t) 0;
}


Matchmaker::
~Matchmaker()
{
	if (AccountantHost) free (AccountantHost);
	delete rankCondStd;
	delete rankCondPrioPreempt;
	delete PreemptionReq;
	delete PreemptionRank;
	delete NegotiatorPreJobRank;
	delete NegotiatorPostJobRank;
	delete sockCache;
	if (Collectors) {
		delete Collectors;
	}
	if (MatchList) {
		delete MatchList;
	}
	if ( cachedName ) free(cachedName);
	if ( cachedAddr ) free(cachedAddr);
}


void Matchmaker::
initialize ()
{
	// read in params
	reinitialize ();

    // register commands
    daemonCore->Register_Command (RESCHEDULE, "Reschedule", 
            (CommandHandlercpp) &Matchmaker::RESCHEDULE_commandHandler, 
			"RESCHEDULE_commandHandler", (Service*) this, DAEMON);
    daemonCore->Register_Command (RESET_ALL_USAGE, "ResetAllUsage",
            (CommandHandlercpp) &Matchmaker::RESET_ALL_USAGE_commandHandler, 
			"RESET_ALL_USAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (RESET_USAGE, "ResetUsage",
            (CommandHandlercpp) &Matchmaker::RESET_USAGE_commandHandler, 
			"RESET_USAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (DELETE_USER, "DeleteUser",
            (CommandHandlercpp) &Matchmaker::DELETE_USER_commandHandler, 
			"DELETE_USER_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_PRIORITYFACTOR, "SetPriorityFactor",
            (CommandHandlercpp) &Matchmaker::SET_PRIORITYFACTOR_commandHandler, 
			"SET_PRIORITYFACTOR_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (SET_PRIORITY, "SetPriority",
            (CommandHandlercpp) &Matchmaker::SET_PRIORITY_commandHandler, 
			"SET_PRIORITY_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (GET_PRIORITY, "GetPriority",
		(CommandHandlercpp) &Matchmaker::GET_PRIORITY_commandHandler, 
			"GET_PRIORITY_commandHandler", this, READ);
    daemonCore->Register_Command (GET_RESLIST, "GetResList",
		(CommandHandlercpp) &Matchmaker::GET_RESLIST_commandHandler, 
			"GET_RESLIST_commandHandler", this, READ);
#ifdef WANT_NETMAN
	daemonCore->Register_Command (REQUEST_NETWORK, "RequestNetwork",
	    (CommandHandlercpp) &Matchmaker::REQUEST_NETWORK_commandHandler,
			"REQUEST_NETWORK_commandHandler", this, WRITE);
#endif

	// Set a timer to renegotiate.
    negotiation_timerID = daemonCore->Register_Timer (0,  NegotiatorInterval,
			(TimerHandlercpp) &Matchmaker::negotiationTime, 
			"Time to negotiate", this);

}

int Matchmaker::
reinitialize ()
{
	char *tmp;

    // Initialize accountant params
    accountant.Initialize();

	// get timeout values

	tmp = param("NEGOTIATOR_INTERVAL");
	if( tmp ) {
		NegotiatorInterval = atoi(tmp);
		free( tmp );
	} else {
		NegotiatorInterval = 300;
	}

	tmp = param("NEGOTIATOR_TIMEOUT");
	if( tmp ) {
		NegotiatorTimeout = atoi(tmp);
		free( tmp );
	} else {
		NegotiatorTimeout = 30;
	}

	delete sockCache;
	tmp = param("NEGOTIATOR_SOCKET_CACHE_SIZE");
	if (tmp) {
		int size = atoi(tmp);
		dprintf (D_ALWAYS,"NEGOTIATOR_SOCKET_CACHE_SIZE = %d\n", size);
		sockCache = new SocketCache(size);
		free(tmp);
	} else {
		sockCache = new SocketCache;
	}

	// get PreemptionReq expression
	if (PreemptionReq) delete PreemptionReq;
	PreemptionReq = NULL;
	tmp = param("PREEMPTION_REQUIREMENTS");
	if( tmp ) {
		if( Parse(tmp, PreemptionReq) ) {
			EXCEPT ("Error parsing PREEMPTION_REQUIREMENTS expression: %s",
					tmp);
		}
	}

	dprintf (D_ALWAYS,"ACCOUNTANT_HOST = %s\n", AccountantHost ? 
			AccountantHost : "None (local)");
	dprintf (D_ALWAYS,"NEGOTIATOR_INTERVAL = %d sec\n",NegotiatorInterval);
	dprintf (D_ALWAYS,"NEGOTIATOR_TIMEOUT = %d sec\n",NegotiatorTimeout);
	dprintf (D_ALWAYS,"PREEMPTION_REQUIREMENTS = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (PreemptionRank) delete PreemptionRank;
	PreemptionRank = NULL;
	tmp = param("PREEMPTION_RANK");
	if( tmp ) {
		if( Parse(tmp, PreemptionRank) ) {
			EXCEPT ("Error parsing PREEMPTION_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"PREEMPTION_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (NegotiatorPreJobRank) delete NegotiatorPreJobRank;
	NegotiatorPreJobRank = NULL;
	tmp = param("NEGOTIATOR_PRE_JOB_RANK");
	if( tmp ) {
		if( Parse(tmp, NegotiatorPreJobRank) ) {
			EXCEPT ("Error parsing NEGOTIATOR_PRE_JOB_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"NEGOTIATOR_PRE_JOB_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

	if (NegotiatorPostJobRank) delete NegotiatorPostJobRank;
	NegotiatorPostJobRank = NULL;
	tmp = param("NEGOTIATOR_POST_JOB_RANK");
	if( tmp ) {
		if( Parse(tmp, NegotiatorPostJobRank) ) {
			EXCEPT ("Error parsing NEGOTIATOR_POST_JOB_RANK expression: %s", tmp);
		}
	}

	dprintf (D_ALWAYS,"NEGOTIATOR_POST_JOB_RANK = %s\n", (tmp?tmp:"None"));

	if( tmp ) free( tmp );

#ifdef WANT_NETMAN
	netman.Config();
#endif

	if (Collectors) {
		delete Collectors;
	}
	Collectors = CollectorList::createForNegotiator();

	want_simple_matching = param_boolean("NEGOTIATOR_SIMPLE_MATCHING",false);
	want_matchlist_caching = param_boolean("NEGOTIATOR_MATCHLIST_CACHING",false);
	ConsiderPreemption = param_boolean("NEGOTIATOR_CONSIDER_PREEMPTION",true);

	// done
	return TRUE;
}


int Matchmaker::
RESCHEDULE_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read eom\n");
		return FALSE;
	}

	if (GotRescheduleCmd) return TRUE;
	GotRescheduleCmd=true;
	daemonCore->Reset_Timer(negotiation_timerID,0,
							NegotiatorInterval);
	return TRUE;
}


int Matchmaker::
RESET_ALL_USAGE_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read eom\n");
		return FALSE;
	}

	// reset usage
	dprintf (D_ALWAYS,"Resetting the usage of all users\n");
	accountant.ResetAllUsage();
	
	return TRUE;
}

int Matchmaker::
DELETE_USER_commandHandler (int, Stream *strm)
{
	char	scheddName[64];
	char	*sn = scheddName;
	int		len = 64;

	// read the required data off the wire
	if (!strm->get(sn, len) 	|| 
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read accountant record name\n");
		return FALSE;
	}

	// reset usage
	dprintf (D_ALWAYS,"Deleting accountanting record of %s\n",scheddName);
	accountant.DeleteRecord (scheddName);
	
	return TRUE;
}

int Matchmaker::
RESET_USAGE_commandHandler (int, Stream *strm)
{
	char	scheddName[64];
	char	*sn = scheddName;
	int		len = 64;

	// read the required data off the wire
	if (!strm->get(sn, len) 	|| 
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read schedd name\n");
		return FALSE;
	}

	// reset usage
	dprintf (D_ALWAYS,"Resetting the usage of %s\n",scheddName);
	accountant.ResetAccumulatedUsage (scheddName);
	
	return TRUE;
}


int Matchmaker::
SET_PRIORITYFACTOR_commandHandler (int, Stream *strm)
{
	float	priority;
	char	scheddName[64];
	char	*sn = scheddName;
	int		len = 64;

	// read the required data off the wire
	if (!strm->get(sn, len) 	|| 
		!strm->get(priority) 	|| 
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read schedd name and priority\n");
		return FALSE;
	}

	// set the priority
	dprintf (D_ALWAYS,"Setting the priority factor of %s to %f\n",scheddName,priority);
	accountant.SetPriorityFactor (scheddName, priority);
	
	return TRUE;
}


int Matchmaker::
SET_PRIORITY_commandHandler (int, Stream *strm)
{
	float	priority;
	char	scheddName[64];
	char	*sn = scheddName;
	int		len = 64;

	// read the required data off the wire
	if (!strm->get(sn, len) 	|| 
		!strm->get(priority) 	|| 
		!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not read schedd name and priority\n");
		return FALSE;
	}

	// set the priority
	dprintf (D_ALWAYS,"Setting the priority of %s to %f\n",scheddName,priority);
	accountant.SetPriority (scheddName, priority);
	
	return TRUE;
}


int Matchmaker::
GET_PRIORITY_commandHandler (int, Stream *strm)
{
	// read the required data off the wire
	if (!strm->end_of_message())
	{
		dprintf (D_ALWAYS, "GET_PRIORITY: Could not read eom\n");
		return FALSE;
	}

	// get the priority
	AttrList* ad=accountant.ReportState();
	dprintf (D_ALWAYS,"Getting state information from the accountant\n");
	
	if (!ad->put(*strm) ||
	    !strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not send priority information\n");
		delete ad;
		return FALSE;
	}

	delete ad;

	return TRUE;
}


int Matchmaker::
GET_RESLIST_commandHandler (int, Stream *strm)
{
    char    scheddName[64];
    char    *sn = scheddName;
    int     len = 64;

    // read the required data off the wire
    if (!strm->get(sn, len)     ||
        !strm->end_of_message())
    {
        dprintf (D_ALWAYS, "Could not read schedd name\n");
        return FALSE;
    }

    // reset usage
    dprintf (D_ALWAYS,"Getting resource list of %s\n",scheddName);

	// get the priority
	AttrList* ad=accountant.ReportState(scheddName);
	dprintf (D_ALWAYS,"Getting state information from the accountant\n");
	
	if (!ad->put(*strm) ||
	    !strm->end_of_message())
	{
		dprintf (D_ALWAYS, "Could not send resource list\n");
		delete ad;
		return FALSE;
	}

	delete ad;

	return TRUE;
}

#ifdef WANT_NETMAN
int Matchmaker::
REQUEST_NETWORK_commandHandler (int, Stream *stream)
{
	return netman.HandleNetworkRequest(stream);
}
#endif

/*
Look for an ad matching the given constraint string
in the table given by arg.  Return a duplicate on success.
Otherwise, return 0.
*/
#if 0
static ClassAd * lookup_global( const char *constraint, void *arg )
{
	ClassAdList *list = (ClassAdList*) arg;
	ClassAd *ad;
	ClassAd queryAd;

	if ( want_simple_matching ) {
		return 0;
	}

	CondorQuery query(ANY_AD);
	query.addANDConstraint(constraint);
	query.getQueryAd(queryAd);
	queryAd.SetTargetTypeName (ANY_ADTYPE);

	list->Open();
	while( (ad = list->Next()) ) {
		if(queryAd <= *ad) {
			return new ClassAd(*ad);
		}
	}

	return 0;
}
#endif

int Matchmaker::
negotiationTime ()
{
	ClassAdList startdAds;
	ClassAdList startdPvtAds;
	ClassAdList scheddAds;
	ClassAdList allAds;

	// Check if we just finished a cycle less than 20 seconds ago.
	// If we did, reset our timer so at least 20 seconds will elapse
	// between cycles.  We do this to help ensure all the startds have
	// had time to update the collector after the last negotiation cycle
	// (otherwise, we might match the same resource twice).  Note: we must
	// do this check _before_ we reset GotRescheduledCmd to false to prevent
	// postponing a new cycle indefinitely.
	unsigned int elapsed = time(NULL) - completedLastCycleTime;
	if ( elapsed < 20 ) {
		daemonCore->Reset_Timer(negotiation_timerID,
							20 - elapsed,
							NegotiatorInterval);
		dprintf(D_FULLDEBUG,
			"New cycle requested but just finished one -- delaying %u secs\n",
			20 - elapsed);
		return FALSE;
	}

	dprintf( D_ALWAYS, "---------- Started Negotiation Cycle ----------\n" );

	GotRescheduleCmd=false;  // Reset the reschedule cmd flag

#ifdef WANT_NETMAN
	netman.PrepareForSchedulingCycle();
#endif

	// ----- Get all required ads from the collector
	dprintf( D_ALWAYS, "Phase 1:  Obtaining ads from collector ...\n" );
	if( !obtainAdsFromCollector( allAds, startdAds, scheddAds,
		startdPvtAds ) )
	{
		dprintf( D_ALWAYS, "Aborting negotiation cycle\n" );
		// should send email here
		return FALSE;
	}

	// Register a lookup function that passes through the list of all ads.	
	// ClassAdLookupRegister( lookup_global, &allAds );
	
	// ----- Recalculate priorities for schedds
	dprintf( D_ALWAYS, "Phase 2:  Performing accounting ...\n" );
	accountant.UpdatePriorities();
	accountant.CheckMatches( startdAds );
	// if don't care about preemption, we can trim out all non Unclaimed ads now.
	// note: we cannot trim out the Unclaimed ads before we call CheckMatches,
	// otherwise CheckMatches will do the wrong thing (because it will not see
	// any of the claimed machines!).
	int num_trimmed = trimStartdAds(startdAds);
	if ( num_trimmed > 0 ) {
		dprintf(D_FULLDEBUG,
			"Trimmed out %d startd ads not Unclaimed\n",num_trimmed);
	} else {
		// for ads which have RemoteUser set, add RemoteUserPrio
		addRemoteUserPrios( startdAds ); 
	}

	char *groups = param("GROUP_NAMES");
	if ( groups ) {

		// HANDLE GROUPS (as desired by CDF)

		// Populate the groupArray, which contains an entry for
		// each group.
		SimpleGroupEntry* groupArray;
		int i;
		StringList groupList;		
		strlwr(groups); // the accountant will want lower case!!!
		groupList.initializeFromString(groups);
		free(groups);		
		groupArray = new SimpleGroupEntry[ groupList.number() ];
		ASSERT(groupArray);

		MyString tmpstr;
		i = 0;
		groupList.rewind();
		while ((groups = groupList.next ()))
		{
			tmpstr.sprintf("GROUP_QUOTA_%s",groups);
			int quota = param_integer(tmpstr.Value(),0);
			if ( !quota ) {
				dprintf(D_ALWAYS,
					"ERROR - no quota specified for group %s, ignoring\n",
					groups);
				continue;
			}
			int usage  = accountant.GetResourcesUsed(groups);
			groupArray[i].groupName = groups;  // don't free this! (in groupList)
			groupArray[i].maxAllowed = quota;
			groupArray[i].usage = usage;
				// the 'prio' field is used to sort the group array, i.e. to
				// decide which groups get to negotiate first.  
				// we sort groups based upon the percentage of their quota
				// currently being used, so that groups using the least 
				// percentage amount of their quota get to negotiate first.
			groupArray[i].prio = ( 100 * usage ) / quota;
			dprintf(D_FULLDEBUG,
				"Group Table : group %s quota %d usage %d prio %2.2f\n",
				groups,quota,usage,groupArray[i].prio);
			i++;
		}
		int groupArrayLen = i;

			// pull out the submitter ads that specify a group from the
			// scheddAds list, and insert them into a list specific to 
			// the specified group.
		ClassAd *ad = NULL;
		char scheddName[80];
		scheddAds.Open();
		while( (ad=scheddAds.Next()) ) {
			if (!ad->LookupString(ATTR_NAME, scheddName, sizeof(scheddName))) {
				continue;
			}
			scheddName[79] = '\0'; // make certain we have a terminating NULL
			char *sep = strchr(scheddName,'.');	// is there a group seperator?
			if ( !sep ) {
				continue;
			}
			*sep = '\0';
			for (i=0; i<groupArrayLen; i++) {
				if ( strcasecmp(scheddName,groupArray[i].groupName)==0 ) {
					groupArray[i].submitterAds.Insert(ad);
					scheddAds.Delete(ad);
					break;
				}
			}
		}

			// now sort the group array
		qsort(groupArray,groupArrayLen,sizeof(SimpleGroupEntry),groupSortCompare);		

			// and negotiate for each group
		for (i=0;i<groupArrayLen;i++) {
			if ( groupArray[i].submitterAds.MyLength() == 0 ) {
				dprintf(D_ALWAYS,
					"Group %s - skipping, no submitters\n",
					groupArray[i].groupName);
				continue;
			}
			if ( groupArray[i].usage >= groupArray[i].maxAllowed  &&
				 !ConsiderPreemption ) 
			{
				dprintf(D_ALWAYS,
					"Group %s - skipping, at or over quota (usage=%d)\n",
					groupArray[i].groupName,groupArray[i].usage);
				continue;
			}
			dprintf(D_ALWAYS,"Group %s - negotiating\n",
				groupArray[i].groupName);
			negotiateWithGroup( startdAds, 
				startdPvtAds, groupArray[i].submitterAds, 
				groupArray[i].maxAllowed, groupArray[i].groupName);
		}

			// if GROUP_AUTOREGROUP is set to true, then for any submitter
			// assigned to a group that did match, insert the submitter
			// ad back into the main scheddAds list.  this way, we will
			// try to match it again below .
		if ( param_boolean("GROUP_AUTOREGROUP",false) )  {
			for (i=0; i<groupArrayLen; i++) {
				ad = NULL;
				dprintf(D_ALWAYS,
					"Group %s - autoregroup inserting %d submitters\n",
					groupArray[i].groupName,
					groupArray[i].submitterAds.MyLength());
				groupArray[i].submitterAds.Open();
				while( (ad=groupArray[i].submitterAds.Next()) ) {
					scheddAds.Insert(ad);				
				}
			}
		}

			// finally, cleanup 
		delete []  groupArray;
		groupArray = NULL;

			// print out a message stating we are about to negotiate below w/
			// all users who did not specify a group
		dprintf(D_ALWAYS,"Group *none* - negotiating\n");

	} // if (groups)
	
		// negotiate w/ all users who do not belong to a group.
	negotiateWithGroup(startdAds, startdPvtAds, scheddAds);
	
	// ----- Done with the negotiation cycle
	dprintf( D_ALWAYS, "---------- Finished Negotiation Cycle ----------\n" );

	completedLastCycleTime = time(NULL);

	return TRUE;
}

Matchmaker::SimpleGroupEntry::
SimpleGroupEntry()
{
	groupName = NULL;
	prio = 0;
	maxAllowed = INT_MAX;
}

Matchmaker::SimpleGroupEntry::
~SimpleGroupEntry()
{
	// Note: don't free groupName!  See comment above.
}

int Matchmaker::
negotiateWithGroup ( ClassAdList& startdAds, ClassAdList& startdPvtAds, 
					 ClassAdList& scheddAds, 
					 int groupQuota, const char* groupAccountingName)
{
	ClassAd		*schedd;
	char		scheddName[80];
	char		scheddAddr[32];
	int			result;
	int			numStartdAds;
	double		maxPrioValue;
	double		maxAbsPrioValue;
	double		normalFactor;
	double		normalAbsFactor;
	double		scheddPrio;
	double		scheddPrioFactor;
	double		scheddShare;
	double		scheddAbsShare;
	int			scheddLimit;
	int			scheddUsage;
	int			MaxscheddLimit;
	int			hit_schedd_prio_limit;
	int			hit_network_prio_limit;
	int 		send_ad_to_schedd;	
	bool ignore_schedd_limit;
	int			num_idle_jobs;

	// ----- Sort the schedd list in decreasing priority order
	dprintf( D_ALWAYS, "Phase 3:  Sorting submitter ads by priority ...\n" );
	scheddAds.Sort( (lessThanFunc)comparisonFunction, this );

	int spin_pie=0;
	do {
#if WANT_NETMAN
		allocNetworkShares = true;
		if (spin_pie && !hit_schedd_prio_limit) {
				// If this is not our first pie spin and we didn't hit
				// a CPU limit for any schedds on our last spin, then
				// we're spinning again because all remaining schedds
				// have been allocated their network fair-share, and
				// they want more network capacity.  We don't want to
				// under-allocate the network, so let them have any
				// remaining network capacity in priority order.
			allocNetworkShares = false;
		}
#endif
		spin_pie++;
		hit_schedd_prio_limit = FALSE;
		hit_network_prio_limit = FALSE;
		calculateNormalizationFactor( scheddAds, maxPrioValue, normalFactor,
									  maxAbsPrioValue, normalAbsFactor);
		numStartdAds = startdAds.MyLength();
			// If operating on a group with a quota, consider the size of 
			// the "pie" to be limited to the groupQuota, so each user in 
			// the group gets a reasonable sized slice.
		if ( numStartdAds > groupQuota ) {
			numStartdAds = groupQuota;
		}
		MaxscheddLimit = 0;
		// ----- Negotiate with the schedds in the sorted list
		dprintf( D_ALWAYS, "Phase 4.%d:  Negotiating with schedds ...\n",
			spin_pie );
		dprintf (D_FULLDEBUG, "    NumStartdAds = %d\n", numStartdAds);
		dprintf (D_FULLDEBUG, "    NormalFactor = %f\n", normalFactor);
		dprintf (D_FULLDEBUG, "    MaxPrioValue = %f\n", maxPrioValue);
		dprintf (D_FULLDEBUG, "    NumScheddAds = %d\n", scheddAds.MyLength());
		scheddAds.Open();
		while( (schedd = scheddAds.Next()) )
		{
			// get the name and address of the schedd
			if( !schedd->LookupString( ATTR_NAME, scheddName ) ||
				!schedd->LookupString( ATTR_SCHEDD_IP_ADDR, scheddAddr ) )
			{
				dprintf (D_ALWAYS,"  Error!  Could not get %s and %s from ad\n",
							ATTR_NAME, ATTR_SCHEDD_IP_ADDR);
				return FALSE;
			}	
			num_idle_jobs = 0;
			schedd->LookupInteger(ATTR_IDLE_JOBS,num_idle_jobs);
			if ( num_idle_jobs < 0 ) {
				num_idle_jobs = 0;
			}

			if ( num_idle_jobs > 0 ) {
				dprintf(D_ALWAYS,"  Negotiating with %s at %s\n",scheddName,
					scheddAddr);
			}

			// should we send the startd ad to this schedd?
			send_ad_to_schedd = FALSE;
			schedd->LookupBool( ATTR_WANT_RESOURCE_AD, send_ad_to_schedd);
	
			// calculate the percentage of machines that this schedd can use
			scheddPrio = accountant.GetPriority ( scheddName );
			scheddUsage = accountant.GetResourcesUsed ( scheddName );
			scheddShare = maxPrioValue/(scheddPrio*normalFactor);
			if ( param_boolean("NEGOTIATOR_IGNORE_USER_PRIORITIES",false) ) {
				scheddLimit = 500000;
			} else {
				scheddLimit  = (int) rint((scheddShare*numStartdAds)-scheddUsage);
			}
			if( scheddLimit < 0 ) {
				scheddLimit = 0;
			}
			if ( groupAccountingName ) {
				int maxAllowed = groupQuota - accountant.GetResourcesUsed(groupAccountingName);
				if ( maxAllowed < 0 ) maxAllowed = 0;
				if ( scheddLimit > maxAllowed ) {
					scheddLimit = maxAllowed;
				}
			}
			if (scheddLimit>MaxscheddLimit) MaxscheddLimit=scheddLimit;

			// calculate this schedd's absolute fair-share for allocating
			// resources other than CPUs (like network capacity and licenses)
			scheddPrioFactor = accountant.GetPriorityFactor ( scheddName );
			scheddAbsShare =
				maxAbsPrioValue/(scheddPrioFactor*normalAbsFactor);

			// print some debug info
			if ( num_idle_jobs > 0 ) {
				dprintf (D_FULLDEBUG, "  Calculating schedd limit with the "
					"following parameters\n");
				dprintf (D_FULLDEBUG, "    ScheddPrio       = %f\n",
					scheddPrio);
				dprintf (D_FULLDEBUG, "    ScheddPrioFactor = %f\n",
					 scheddPrioFactor);
				dprintf (D_FULLDEBUG, "    scheddShare      = %f\n",
					scheddShare);
				dprintf (D_FULLDEBUG, "    scheddAbsShare   = %f\n",
					scheddAbsShare);
				dprintf (D_FULLDEBUG, "    ScheddUsage      = %d\n",
					scheddUsage);
				dprintf (D_FULLDEBUG, "    scheddLimit      = %d\n",
					scheddLimit);
				dprintf (D_FULLDEBUG, "    MaxscheddLimit   = %d\n",
					MaxscheddLimit);
			}

			// initialize reasons for match failure; do this now
			// in case we never actually call negotiate() below.
			rejForNetwork = 0;
			rejForNetworkShare = 0;
			rejPreemptForPrio = 0;
			rejPreemptForPolicy = 0;
			rejPreemptForRank = 0;

			// Optimizations: 
			// If number of idle jobs = 0, don't waste time with negotiate.
			// Likewise, if limit is 0, don't waste time with negotiate EXCEPT
			// on the first spin of the pie (spin_pie==1), we must
			// still negotiate because on the first spin we tell the negotiate
			// function to ignore the scheddLimit w/ respect to jobs which
			// are strictly preferred by resource offers (via startd rank).
			if ( num_idle_jobs == 0 ) {
				dprintf(D_FULLDEBUG,
					"  Negotiating with %s skipped because no idle jobs\n",
					scheddName);
				result = MM_DONE;
			} else {
				if ( scheddLimit < 1 && spin_pie > 1 ) {
					result = MM_RESUME;
				} else {
					if ( spin_pie == 1 && ConsiderPreemption ) {
						ignore_schedd_limit = true;
					} else {
						ignore_schedd_limit = false;
					}
					result=negotiate( scheddName,scheddAddr,scheddPrio,
								  scheddAbsShare, scheddLimit,
								  startdAds, startdPvtAds, 
								  send_ad_to_schedd,ignore_schedd_limit);
				}
			}

			switch (result)
			{
				case MM_RESUME:
					// the schedd hit its resource limit.  must resume 
					// negotiations at a later negotiation cycle.
					dprintf(D_FULLDEBUG,
							"  This schedd hit its scheddlimit.\n");
					hit_schedd_prio_limit = TRUE;
					break;
				case MM_DONE: 
					if (rejForNetworkShare) {
							// We negotiated for all jobs, but some
							// jobs were rejected because this user
							// exceeded her fair-share of network
							// resources.  Resume negotiations for
							// this user at a later cycle.
						hit_network_prio_limit = TRUE;
					} else {
							// the schedd got all the resources it
							// wanted. delete this schedd ad.
						dprintf(D_FULLDEBUG,"  Schedd %s got all it wants; "
								"removing it.\n", scheddName );
						scheddAds.Delete( schedd);
					}
					break;
				case MM_ERROR:
				default:
					dprintf(D_ALWAYS,"  Error: Ignoring schedd for this cycle\n" );
					sockCache->invalidateSock( scheddAddr );
					scheddAds.Delete( schedd );
			}
		}
		scheddAds.Close();
	} while ( (hit_schedd_prio_limit == TRUE || hit_network_prio_limit == TRUE)
			 && (MaxscheddLimit > 0) && (startdAds.MyLength() > 0) );

	return TRUE;
}

static int
comparisonFunction (AttrList *ad1, AttrList *ad2, void *m)
{
	char	scheddName1[64];
	char	scheddName2[64];
	double	prio1, prio2;
	Matchmaker *mm = (Matchmaker *) m;



	if (!ad1->LookupString (ATTR_NAME, scheddName1) ||
		!ad2->LookupString (ATTR_NAME, scheddName2))
	{
		return -1;
	}

	prio1 = mm->accountant.GetPriority(scheddName1);
	prio2 = mm->accountant.GetPriority(scheddName2);
	
	// the scheddAds should be secondarily sorted based on ATTR_NAME
	// because we assume in the code that follows that ads with the
	// same ATTR_NAME are adjacent in the scheddAds list.  this is
	// usually the case because 95% of the time each user in the
	// system has a different priority.

	if (prio1==prio2) {
		int namecomp = strcmp(scheddName1,scheddName2);
		if (namecomp != 0) return (namecomp < 0);

			// We don't always want to negotiate with schedds with the
			// same name in the same order or we might end up only
			// running jobs this user has submitted to the first
			// schedd.  The general problem is that we rely on the
			// schedd to order each user's jobs, so when a user
			// submits to multiple schedds, there is no guaranteed
			// order.  Our hack is to order the schedds randomly,
			// which should be a little bit better than always
			// negotiating in the same order.  We use the timestamp on
			// the classads to get a random ordering among the schedds
			// (consistent throughout our sort).

		int ts1=0, ts2=0;
		ad1->LookupInteger (ATTR_LAST_HEARD_FROM, ts1);
		ad2->LookupInteger (ATTR_LAST_HEARD_FROM, ts2);
		return ( (ts1 % 1009) < (ts2 % 1009) );
	}

	return (prio1 < prio2);
}

int Matchmaker::
trimStartdAds(ClassAdList &startdAds)
{
	int removed = 0;
	ClassAd *ad = NULL;
	char curState[80];
	char *unclaimed = state_to_string(unclaimed_state);
	ASSERT(unclaimed);

		// If we are not considering preemption, we can save time
		// (and also make the spinning pie algorithm more correct) by
		// getting rid of ads that are not in the Unclaimed state.
	
	if ( ConsiderPreemption ) {
			// we need to keep all the ads.
		return 0;
	}

	startdAds.Open();
	while( (ad=startdAds.Next()) ) {
		if(ad->LookupString(ATTR_STATE, curState, sizeof(curState))) {
			if ( strcmp(curState,unclaimed) ) {
				startdAds.Delete(ad);
				removed++;
			}
		}
	}
	startdAds.Close();

	return removed;
}

bool Matchmaker::
obtainAdsFromCollector (
						ClassAdList &allAds,
						ClassAdList &startdAds, 
						ClassAdList &scheddAds, 
						ClassAdList &startdPvtAds )
{
	CondorQuery privateQuery(STARTD_PVT_AD);
	QueryResult result;
	ClassAd *ad, *oldAd;
	MapEntry *oldAdEntry;
	int newSequence, oldSequence, reevaluate_ad;
	char    remoteHost[MAXHOSTNAMELEN];

	if ( want_simple_matching ) {
		CondorQuery publicQuery(STARTD_AD);
		CondorQuery submittorQuery(SUBMITTOR_AD);

		dprintf(D_ALWAYS, "  Getting all startd ads ...\n");
		result = Collectors->query(publicQuery,startdAds);
		if( result!=Q_OK ) {
			dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
			return false;
		}

		dprintf(D_ALWAYS, "  Getting all submittor ads ...\n");
		result = Collectors->query(submittorQuery,scheddAds);
		if( result!=Q_OK ) {
			dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
			return false;
		}

		dprintf(D_ALWAYS,"  Getting startd private ads ...\n");
		result = Collectors->query(privateQuery,startdPvtAds);
		if( result!=Q_OK ) {
			dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
			return false;
		}

		dprintf(D_ALWAYS, 
			"Got ads (simple matching): %d startd, %d submittor, %d private\n",
	        startdAds.MyLength(),scheddAds.MyLength(),startdPvtAds.MyLength());

		return true;
	}

	CondorQuery publicQuery(ANY_AD);
	dprintf(D_ALWAYS, "  Getting all public ads ...\n");
	result = Collectors->query (publicQuery, allAds);
	if( result!=Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
		return false;
	}

	dprintf(D_ALWAYS, "  Sorting %d ads ...\n",allAds.MyLength());

	allAds.Open();
	while( (ad=allAds.Next()) ) {

		// Insert each ad into the appropriate list.
		// After we insert it into a list, do not delete the ad...

		// let's see if we've already got it - first lookup the sequence 
		// number from the new ad, then let's look and see if we've already
		// got something for this one.		
		if(!strcmp(ad->GetMyTypeName(),STARTD_ADTYPE)) {

			if(!ad->LookupString(ATTR_NAME, remoteHost)) {
				dprintf(D_FULLDEBUG,"Rejecting unnamed startd ad.");
				continue;
			}


			// first, let's make sure that will want to actually use this
			// ad, and if we can use it (old startds had no seq. number)
			reevaluate_ad = false; 
			ad->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
			newSequence = -1;	
			ad->LookupInteger(ATTR_UPDATE_SEQUENCE_NUMBER, newSequence);

			if( reevaluate_ad && newSequence != -1 ) {
				oldAd = NULL;
				oldAdEntry = NULL;
				MyString rhost(remoteHost);
				stashedAds->lookup( rhost, oldAdEntry);
				// if we find it...
				oldSequence = -1;
				if( oldAdEntry ) {
					oldSequence = oldAdEntry->sequenceNum;
				}
				if(newSequence > oldSequence) {
					if(oldSequence >= 0) {
						delete(oldAdEntry->oldAd);
						delete(oldAdEntry->remoteHost);
						delete(oldAdEntry);
						stashedAds->remove(rhost);
					}
					MapEntry *me = new MapEntry;
					me->sequenceNum = newSequence;
					me->remoteHost = strdup(remoteHost);
					me->oldAd = new ClassAd(*ad); 
					stashedAds->insert(rhost, me); 
				} else {
					/*
					  We have a stashed copy of this ad, and it's the
					  the same or a more recent sequence number, and we
					  we don't want to use the one in allAds. However, 
					  we need to make sure that the "stashed" ad gets into
					  allAds for this negotiation cycle, but we don't want 
					  to get stuck in a loop evaluating the, so we remove
					  the sequence number before we put it into allAds - this
					  way, when we encounter it a few iteration later we
					  won't reconsider it
					*/

					allAds.Delete(ad);
					ad = new ClassAd(*(oldAdEntry->oldAd));
					ad->Delete(ATTR_UPDATE_SEQUENCE_NUMBER);
					allAds.Insert(ad);
				}
			}
			startdAds.Insert(ad);
		} else if( !strcmp(ad->GetMyTypeName(),SUBMITTER_ADTYPE) ) {
			scheddAds.Insert(ad);
		}
	}
	allAds.Close();

	dprintf(D_ALWAYS,"  Getting startd private ads ...\n");
	result = Collectors->query (privateQuery, startdPvtAds);
	if( result!=Q_OK ) {
		dprintf(D_ALWAYS, "Couldn't fetch ads: %s\n", getStrQueryResult(result));
		return false;
	}

	dprintf(D_ALWAYS, "Got ads: %d public and %d private\n",
	        allAds.MyLength(),startdPvtAds.MyLength());

	dprintf(D_ALWAYS, "Public ads include %d submitter, %d startd\n",
		scheddAds.MyLength(), startdAds.MyLength() );

	return true;
}


int Matchmaker::
negotiate( char *scheddName, char *scheddAddr, double priority, double share,
		   int scheddLimit,
		   ClassAdList &startdAds, ClassAdList &startdPvtAds, 
		   int send_ad_to_schedd, bool ignore_schedd_limit)
{
	ReliSock	*sock;
	int			i;
	int			reply;
	int			cluster, proc;
	int			result;
	ClassAd		request;
	ClassAd		*offer;
	bool		only_consider_startd_rank;
	bool		display_overlimit = true;
	char		prioExpr[128], remoteUser[128];

	// 0.  connect to the schedd --- ask the cache for a connection
	sock = sockCache->findReliSock( scheddAddr );
	if( ! sock ) {
		dprintf( D_FULLDEBUG, "Socket to %s not in cache, creating one\n", 
				 scheddAddr );
			// not in the cache already, create a new connection and
			// add it to the cache.  We want to use a Daemon object to
			// send the first command so we setup a security session. 
		Daemon schedd( DT_SCHEDD, scheddAddr, 0 );
		sock = schedd.reliSock( NegotiatorTimeout );
		if( ! sock ) {
			dprintf( D_ALWAYS, "    Failed to connect to %s\n", scheddAddr );
				// invalidateSock() might be unecessary, but doesn't hurt...
			sockCache->invalidateSock( scheddAddr );
			return MM_ERROR;
		}
		if( ! schedd.startCommand(NEGOTIATE, sock, NegotiatorTimeout) ) {
			dprintf( D_ALWAYS, "    Failed to send NEGOTIATE to %s\n",
					 scheddAddr );
			sockCache->invalidateSock( scheddAddr );
			return MM_ERROR;
		}
			// finally, add it to the cache for later...
		sockCache->addReliSock( scheddAddr, sock );
	} else { 
		dprintf( D_FULLDEBUG, "Socket to %s already in cache, reusing\n", 
				 scheddAddr );
			// this address is already in our socket cache.  since
			// we've already got a TCP connection, we do *NOT* want to
			// use a Daemon::startCommand() to create a new security
			// session, we just want to encode the NEGOTIATE int on
			// the socket...
		sock->encode();
		if( ! sock->put(NEGOTIATE) ) {
			dprintf( D_ALWAYS, "    Failed to send NEGOTIATE to %s\n",
					 scheddAddr );
			sockCache->invalidateSock( scheddAddr );
			return MM_ERROR;
		}
	}

	// 1.  send NEGOTIATE command, followed by the scheddName (user@uiddomain)
	sock->encode();
	if (!sock->put(scheddName) || !sock->end_of_message())
	{
		dprintf (D_ALWAYS, "    Failed to send scheddName/eom\n");
		sockCache->invalidateSock(scheddAddr);
		return MM_ERROR;
	}

	// setup expression with the submittor's priority
	(void) sprintf( prioExpr , "%s = %f" , ATTR_SUBMITTOR_PRIO , priority );

	// 2.  negotiation loop with schedd
	// for (i = 0; i < scheddLimit;  i++)
	for (i=0;true;i++)
	{
		// Service any interactive commands on our command socket.
		// This keeps condor_userprio hanging to a minimum when
		// we are involved in a lot of schedd negotiating.
		// It also performs the important function of draining out
		// any reschedule requests queued up on our command socket, so
		// we do not negotiate over & over unnecesarily.
		daemonCore->ServiceCommandSocket();

		// Handle the case if we are over the scheddLimit
		if ( i >= scheddLimit ) {
			if ( ignore_schedd_limit ) {
				only_consider_startd_rank = true;
				if ( display_overlimit ) {  // print message only once
					display_overlimit = false;
					dprintf (D_FULLDEBUG, 	
						"    Over submitter resource limit (%d) ... "
					    "only consider startd ranks\n", scheddLimit);
				}
			} else {
				dprintf (D_ALWAYS, 	
				"    Reached submitter resource limit: %d ... stopping\n", i);
				break;	// get out of the infinite for loop & stop negotiating
			}
		} else {
			only_consider_startd_rank = false;
		}


		// 2a.  ask for job information
		dprintf (D_FULLDEBUG, "    Sending SEND_JOB_INFO/eom\n");
		sock->encode();
		if (!sock->put(SEND_JOB_INFO) || !sock->end_of_message())
		{
			dprintf (D_ALWAYS, "    Failed to send SEND_JOB_INFO/eom\n");
			sockCache->invalidateSock(scheddAddr);
			return MM_ERROR;
		}

		// 2b.  the schedd may either reply with JOB_INFO or NO_MORE_JOBS
		dprintf (D_FULLDEBUG, "    Getting reply from schedd ...\n");
		sock->decode();
		if (!sock->get (reply))
		{
			dprintf (D_ALWAYS, "    Failed to get reply from schedd\n");
			sock->end_of_message ();
            sockCache->invalidateSock(scheddAddr);
			return MM_ERROR;
		}

		// 2c.  if the schedd replied with NO_MORE_JOBS, cleanup and quit
		if (reply == NO_MORE_JOBS)
		{
			dprintf (D_ALWAYS, "    Got NO_MORE_JOBS;  done negotiating\n");
			sock->end_of_message ();
				// If we have negotiated above our scheddLimit, we have only
				// considered matching if the offer strictly prefers the request.
				// So in this case, return MM_RESUME since there still may be 
				// jobs which the schedd wants scheduled but have not been considered
				// as candidates for no preemption or user priority preemption.
			if ( i >= scheddLimit ) {
				return MM_RESUME;
			} else {
				return MM_DONE;
			}
		}
		else
		if (reply != JOB_INFO)
		{
			// something goofy
			dprintf(D_ALWAYS,"    Got illegal command %d from schedd\n",reply);
			sock->end_of_message ();
            sockCache->invalidateSock(scheddAddr);
			return MM_ERROR;
		}

		// 2d.  get the request 
		dprintf (D_FULLDEBUG,"    Got JOB_INFO command; getting classad/eom\n");
		if (!request.initFromStream(*sock) || !sock->end_of_message())
		{
			dprintf(D_ALWAYS, "    JOB_INFO command not followed by ad/eom\n");
			sock->end_of_message();
            sockCache->invalidateSock(scheddAddr);
			return MM_ERROR;
		}
		if (!request.LookupInteger (ATTR_CLUSTER_ID, cluster) ||
			!request.LookupInteger (ATTR_PROC_ID, proc))
		{
			dprintf (D_ALWAYS, "    Could not get %s and %s from request\n",
					ATTR_CLUSTER_ID, ATTR_PROC_ID);
			sockCache->invalidateSock( scheddAddr );
			return MM_ERROR;
		}
		dprintf(D_ALWAYS, "    Request %05d.%05d:\n", cluster, proc);

		// insert the priority expression into the request
		request.Insert( prioExpr );

		// 2e.  find a compatible offer for the request --- keep attempting
		//		to find matches until we can successfully (1) find a match,
		//		AND (2) notify the startd; so quit if we got a MM_GOOD_MATCH,
		//		or if MM_NO_MATCH could be found
		result = MM_BAD_MATCH;
		while (result == MM_BAD_MATCH) 
		{
			// 2e(i).  find a compatible offer
			if (!(offer=matchmakingAlgorithm(scheddName, scheddAddr, request,
											 startdAds, priority,
											 share, only_consider_startd_rank)))
			{
				int want_match_diagnostics = 0;
				request.LookupBool (ATTR_WANT_MATCH_DIAGNOSTICS,
									want_match_diagnostics);
				char *diagnostic_message = NULL;
				// no match found
				dprintf(D_ALWAYS|D_MATCH, "      Rejected %d.%d %s %s: ",
						cluster, proc, scheddName, scheddAddr);
				if (rejForNetwork) {
					diagnostic_message = "insufficient bandwidth";
					dprintf(D_ALWAYS|D_MATCH|D_NOHEADER, "%s\n",
							diagnostic_message);
#if WANT_NETMAN
					netman.ShowDeniedRequests(D_BANDWIDTH);
#endif
				} else {
					if (rejForNetworkShare) {
						diagnostic_message = "network share exceeded";
					} else if (rejPreemptForPolicy) {
						diagnostic_message =
							"PREEMPTION_REQUIREMENTS == False";
					} else if (rejPreemptForPrio) {
						diagnostic_message = "insufficient priority";
					} else {
						diagnostic_message = "no match found";
					}
					dprintf(D_ALWAYS|D_MATCH|D_NOHEADER, "%s\n",
							diagnostic_message);
				}
				sock->encode();
				if ((want_match_diagnostics) ? 
					(!sock->put(REJECTED_WITH_REASON) ||
					 !sock->put(diagnostic_message) ||
					 !sock->end_of_message()) :
					(!sock->put(REJECTED) || !sock->end_of_message()))
					{
						dprintf (D_ALWAYS, "      Could not send rejection\n");
						sock->end_of_message ();
						sockCache->invalidateSock(scheddAddr);
						
						return MM_ERROR;
					}
				result = MM_NO_MATCH;
				continue;
			}

			char	remoteHost[MAXHOSTNAMELEN];
			double	remotePriority;

			if (offer->LookupString(ATTR_REMOTE_USER, remoteUser) == 1)
			{
				offer->LookupString(ATTR_NAME, remoteHost);
				remotePriority = accountant.GetPriority (remoteUser);

				// got a candidate preemption --- print a helpful message
				dprintf( D_ALWAYS, "      Preempting %s (prio=%.2f) on %s "
						 "for %s (prio=%.2f)\n", remoteUser,
						 remotePriority, remoteHost, scheddName,
						 priority );
			} else {
				strcpy(remoteUser, "none");
			}

			// 2e(ii).  perform the matchmaking protocol
			result = matchmakingProtocol (request, offer, startdPvtAds, sock, 
					scheddName, scheddAddr, send_ad_to_schedd);

			// 2e(iii). if the matchmaking protocol failed, do not consider the
			//			startd again for this negotiation cycle.
			if (result == MM_BAD_MATCH)
				startdAds.Delete (offer);

			// 2e(iv).  if the matchmaking protocol failed to talk to the 
			//			schedd, invalidate the connection and return
			if (result == MM_ERROR)
			{
				sockCache->invalidateSock (scheddAddr);
				return MM_ERROR;
			}
		}

		// 2f.  if MM_NO_MATCH was found for the request, get another request
		if (result == MM_NO_MATCH) 
		{
			i--;		// haven't used any resources this cycle
			continue;
		}

		// 2g.  Delete ad from list so that it will not be considered again in 
		//		this negotiation cycle
		int reevaluate_ad = false;
		offer->LookupBool(ATTR_WANT_AD_REVAULATE, reevaluate_ad);
		if( reevaluate_ad ) {
			reeval(offer);
		} else  {
			startdAds.Delete (offer);
		}	
	}


	// break off negotiations
	sock->encode();
	if (!sock->put (END_NEGOTIATE) || !sock->end_of_message())
	{
		dprintf (D_ALWAYS, "    Could not send END_NEGOTIATE/eom\n");
        sockCache->invalidateSock(scheddAddr);
	}

	// ... and continue negotiating with others
	return MM_RESUME;
}

float Matchmaker::
EvalNegotiatorMatchRank(char const *expr_name,ExprTree *expr,
                        ClassAd &request,ClassAd *resource)
{
	EvalResult result;
	float rank = -(FLT_MAX);

	if(expr && expr->EvalTree(resource,&request,&result)) {
		if( result.type == LX_FLOAT ) {
			rank = result.f;
		} else if( result.type == LX_INTEGER ) {
			rank = result.i;
		} else {
			dprintf(D_ALWAYS, "Failed to evaluate %s "
			                  "expression to a float.\n",expr_name);
		}
	} else if(expr) {
		dprintf(D_ALWAYS, "Failed to evaluate %s "
		                  "expression.\n",expr_name);
	}
	return rank;
}


ClassAd *Matchmaker::
matchmakingAlgorithm(char *scheddName, char *scheddAddr, ClassAd &request,
					 ClassAdList &startdAds,
					 double preemptPrio, double share,
					 bool only_for_startdrank)
{
		// to store values pertaining to a particular candidate offer
	ClassAd 		*candidate;
	double			candidateRankValue;
	double			candidatePreJobRankValue;
	double			candidatePostJobRankValue;
	double			candidatePreemptRankValue;
	PreemptState	candidatePreemptState;
		// to store the best candidate so far
	ClassAd 		*bestSoFar = NULL;	
	ClassAd 		*cached_bestSoFar = NULL;	
	double			bestRankValue = -(FLT_MAX);
	double			bestPreJobRankValue = -(FLT_MAX);
	double			bestPostJobRankValue = -(FLT_MAX);
	double			bestPreemptRankValue = -(FLT_MAX);
	PreemptState	bestPreemptState = (PreemptState)-1;
	bool			newBestFound;
		// to store results of evaluations
	char			remoteUser[256];
	EvalResult		result;
	float			tmp;
		// request attributes
	int				requestAutoCluster = -1;

	request.LookupInteger(ATTR_AUTO_CLUSTER_ID, requestAutoCluster);

		// If this incoming job is from the same user, same schedd,
		// and is in the same autocluster, and we have a MatchList cache,
		// then we can just pop off
		// the top entry in our MatchList if we have one.  The 
		// MatchList is essentially just a sorted cache of the machine
		// ads that match jobs of this type (i.e. same autocluster).
	if ( MatchList &&
		 cachedAutoCluster != -1 &&
		 cachedAutoCluster == requestAutoCluster &&
		 strcmp(cachedName,scheddName)==0 &&
		 strcmp(cachedAddr,scheddAddr)==0 )
	{
		// we can use cached information.  pop off the best
		// candidate from our sorted list.
		cached_bestSoFar = MatchList->pop_candidate();
		if ( ! cached_bestSoFar ) {
				// if we don't have a candidate, fill in
				// all the rejection reason counts.
			MatchList->get_diagnostics(
				rejForNetwork,
				rejForNetworkShare,
				rejPreemptForPrio,
				rejPreemptForPolicy,
				rejPreemptForRank);
		}
			//  TODO  - compare results, reserve net bandwidth
		return cached_bestSoFar;
	}

		// Create a new MatchList cache if desired via config file,
		// and if this job request if from an autocluster not already
		// cached.
	if ( (want_matchlist_caching) && (requestAutoCluster != -1) ) {
			// create a new MatchList cache.
		if ( MatchList ) delete MatchList;
		MatchList = new MatchListType( startdAds.Length() );
		cachedAutoCluster = requestAutoCluster;
		if ( cachedName ) free(cachedName);
		if ( cachedAddr ) free(cachedAddr);
		cachedName = strdup(scheddName);
		cachedAddr = strdup(scheddAddr);
	}
	

#ifdef WANT_NETMAN
	// initialize network information for this request
	char scheddIPbuf[128];
	strcpy(scheddIPbuf, scheddAddr);
	char *colon = strchr(scheddIPbuf, ':');
	if (!colon) {
		dprintf(D_ALWAYS, "      Invalid %s: %s\n", ATTR_SCHEDD_IP_ADDR,
				scheddIPbuf);
		return NULL;
	}
	*colon = '\0';
	char *scheddIP = scheddIPbuf+1;	// skip the leading '<'
	int executableSize = 0;
	request.LookupInteger(ATTR_EXECUTABLE_SIZE, executableSize);
	int universe = CONDOR_UNIVERSE_STANDARD;
	int ckptSize = 0;
	request.LookupInteger(ATTR_JOB_UNIVERSE, universe);
	char lastCkptServer[MAXHOSTNAMELEN], lastCkptServerIP[16];
	lastCkptServerIP[0] = '\0';
	if (universe == CONDOR_UNIVERSE_STANDARD) {
		float cputime = 1.0;
		request.LookupFloat(ATTR_JOB_REMOTE_USER_CPU, cputime);
		if (cputime > 0.0) {
			// if job_universe is STANDARD (checkpointing is
			// enabled) and cputime > 0.0 (job has committed
			// some work), then the job will need to read a
			// checkpoint to restart, so we must set ckptSize
			request.LookupInteger(ATTR_IMAGE_SIZE, ckptSize);
			ckptSize -= executableSize;	// imagesize = ckptsize + executablesz
			if (ckptSize > 0) {
				if (request.LookupString(ATTR_LAST_CKPT_SERVER,
										 lastCkptServer)) {
					struct hostent *hp = gethostbyname(lastCkptServer);
					if (!hp) {
						dprintf(D_ALWAYS,
								"      DNS lookup for %s %s failed!\n",
								ATTR_LAST_CKPT_SERVER, lastCkptServer);
					} else {
						strcpy(lastCkptServerIP,
							   inet_ntoa(*((struct in_addr *)hp->h_addr)));
					}
				} else {
					strcpy(lastCkptServerIP, scheddIP);
				}
			}
		}
	}
#endif

	// initialize reasons for match failure
	rejForNetwork = 0;
	rejForNetworkShare = 0;
	rejPreemptForPrio = 0;
	rejPreemptForPolicy = 0;
	rejPreemptForRank = 0;

	// scan the offer ads
	startdAds.Open ();
	while ((candidate = startdAds.Next ())) {

			// the candidate offer and request must match
		if( !( *candidate == request ) ) {
				// they don't match; continue
			continue;
		}

		candidatePreemptState = NO_PREEMPTION;

		remoteUser[0] = '\0';
		candidate->LookupString(ATTR_REMOTE_USER, remoteUser);

		// if only_for_startdrank flag is true, check if the offer strictly
		// prefers this request.  Since this is the only case we care about
		// when the only_for_startdrank flag is set, if the offer does 
		// not prefer it, just continue with the next offer ad....  we can
		// skip all the below logic about preempt for user-priority, etc.
		if ( only_for_startdrank ) {
			if ( remoteUser[0] == '\0' ) {
					// offer does not have a remote user, thus we cannot eval
					// startd rank yet because it does not make sense (the
					// startd has nothing to compare against).  
					// So try the next offer...
				continue;
			}
			if ( !(rankCondStd->EvalTree(candidate, &request, &result) && 
					result.type == LX_INTEGER && result.i == TRUE) ) {
					// offer does not strictly prefer this request.
					// try the next offer since only_for_statdrank flag is set
				continue;
			}
			// If we made it here, we have a candidate which strictly prefers
			// this request.  Set the candidatePreemptState properly so that
			// we consider PREEMPTION_RANK down below as we should.
			candidatePreemptState = RANK_PREEMPTION;
		}

		// if there is a remote user, consider preemption ....
		// Note: we skip this if only_for_startdrank is true since we already
		//       tested above for the only condition we care about.
		if ( (remoteUser[0] != '\0') &&
			 (!only_for_startdrank) ) {
			if( rankCondStd->EvalTree(candidate, &request, &result) && 
					result.type == LX_INTEGER && result.i == TRUE ) {
					// offer strictly prefers this request to the one
					// currently being serviced; preempt for rank
				candidatePreemptState = RANK_PREEMPTION;
			} else if( accountant.GetPriority(remoteUser) >= preemptPrio +
				PriorityDelta ) {
					// RemoteUser on machine has *worse* priority than request
					// so we can preempt this machine *but* we need to check
					// on two things first
				candidatePreemptState = PRIO_PREEMPTION;
					// (1) we need to make sure that PreemptionReq's hold (i.e.,
					// if the PreemptionReq expression isn't true, dont preempt)
				if (PreemptionReq && 
						!(PreemptionReq->EvalTree(candidate,&request,&result) &&
						result.type == LX_INTEGER && result.i == TRUE) ) {
					rejPreemptForPolicy++;
					continue;
				}
					// (2) we need to make sure that the machine ranks the job
					// at least as well as the one it is currently running 
					// (i.e., rankCondPrioPreempt holds)
				if(!(rankCondPrioPreempt->EvalTree(candidate,&request,&result)&&
						result.type == LX_INTEGER && result.i == TRUE ) ) {
						// machine doesn't like this job as much -- find another
					rejPreemptForRank++;
					continue;
				}
			} else {
					// don't have better priority *and* offer doesn't prefer
					// request --- find another machine
				if (strcmp(remoteUser, scheddName)) {
						// only set rejPreemptForPrio if we aren't trying to
						// preempt one of our own jobs!
					rejPreemptForPrio++;
				}
				continue;
			}
		}

#if WANT_NETMAN
			// is network bandwidth available for this match?
		double networkShare = (allocNetworkShares) ? share : 1.0;
		int rval = netman.RequestPlacement(scheddName, networkShare, scheddIP,
										   executableSize, lastCkptServerIP,
										   ckptSize, request, *candidate);
		if (rval == 1) {
			rejForNetworkShare++;
			continue;
		} else if (rval == 0) {
			rejForNetwork++;
			continue;
		}
#endif

		candidatePreJobRankValue = EvalNegotiatorMatchRank(
		  "NEGOTIATOR_PRE_JOB_RANK",NegotiatorPreJobRank,
		  request,candidate);

		// calculate the request's rank of the offer
		if(!request.EvalFloat(ATTR_RANK,candidate,tmp)) {
			tmp = 0.0;
		}
		candidateRankValue = tmp;

		candidatePostJobRankValue = EvalNegotiatorMatchRank(
		  "NEGOTIATOR_POST_JOB_RANK",NegotiatorPostJobRank,
		  request,candidate);

		candidatePreemptRankValue = -(FLT_MAX);
		if(candidatePreemptState != NO_PREEMPTION) {
			candidatePreemptRankValue = EvalNegotiatorMatchRank(
			  "PREEMPTION_RANK",PreemptionRank,
			  request,candidate);
		}

		if ( MatchList ) {
			MatchList->add_candidate(
					candidate,
					candidateRankValue,
					candidatePreJobRankValue,
					candidatePostJobRankValue,
					candidatePreemptRankValue,
					candidatePreemptState
					);
		}

		// NOTE!!!   IF YOU CHANGE THE LOGIC OF THE BELOW LEXICOGRAPHIC
		// SORT, YOU MUST ALSO CHANGE THE LOGIC IN METHOD
   		//     Matchmaker::MatchListType::sort_compare() !!!
		// THIS STATE OF AFFAIRS IS TEMPORARY.  ONCE WE ARE CONVINVED
		// THAT THE MatchList LOGIC IS WORKING PROPERLY, AND AUTOCLUSTERS
		// ARE AUTOMATIC, THEN THE MatchList SORTING WILL ALWAYS BE USED
		// AND THE LEXICOGRAPHIC SORT BELOW WILL BE REMOVED.
		// - Todd Tannenbaum <tannenba@cs.wisc.edu> 10/2004
		// ----------------------------------------------------------
		// the quality of a match is determined by a lexicographic sort on
		// the following values, but more is better for each component
		//  1. negotiator pre job rank
		//  1. job rank of offer 
		//  2. negotiator post job rank
		//	3. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
		//  4. preemption rank (if preempting)

		newBestFound = false;
		if(candidatePreJobRankValue < bestPreJobRankValue);
		else if(candidatePreJobRankValue > bestPreJobRankValue) {
			newBestFound = true;
		}
		else if(candidateRankValue < bestRankValue);
		else if(candidateRankValue > bestRankValue) {
			newBestFound = true;
		}
		else if(candidatePostJobRankValue < bestPostJobRankValue);
		else if(candidatePostJobRankValue > bestPostJobRankValue) {
			newBestFound = true;
		}
		else if(candidatePreemptState < bestPreemptState);
		else if(candidatePreemptState > bestPreemptState) {
			newBestFound = true;
		}
		//NOTE: if NO_PREEMPTION, PreemptRank is a constant
		else if(candidatePreemptRankValue < bestPreemptRankValue);
		else if(candidatePreemptRankValue > bestPreemptRankValue) {
			newBestFound = true;
		}

		if( newBestFound || !bestSoFar ) {
			bestSoFar = candidate;
			bestPreJobRankValue = candidatePreJobRankValue;
			bestRankValue = candidateRankValue;
			bestPostJobRankValue = candidatePostJobRankValue;
			bestPreemptState = candidatePreemptState;
			bestPreemptRankValue = candidatePreemptRankValue;
		}
	}
	startdAds.Close ();

	if ( MatchList ) {
		MatchList->set_diagnostics(rejForNetwork, rejForNetworkShare, 
			rejPreemptForPrio, rejPreemptForPolicy, rejPreemptForRank);
		dprintf(D_FULLDEBUG,"Start of sorting MatchList (len=%d)\n",
			MatchList->length());
		MatchList->sort();
		dprintf(D_FULLDEBUG,"Finished sorting MatchList\n");
		// compare
		ClassAd *bestCached = MatchList->pop_candidate();
		// TODO - do bestCached and bestSoFar refer to the same
		// machine preference? (sanity check)
	}

#if WANT_NETMAN
	if (bestSoFar) {
		// request the network bandwidth for our choice
		netman.RequestPlacement(scheddName, share, scheddIP, executableSize,
								lastCkptServerIP, ckptSize, request,
								*bestSoFar);
	}
#endif

	// this is the best match
	return bestSoFar;
}


int Matchmaker::
matchmakingProtocol (ClassAd &request, ClassAd *offer, 
						ClassAdList &startdPvtAds, Sock *sock,
					    char* scheddName, char* scheddAddr,
						int send_ad_to_schedd)
{
	int  cluster, proc;
	char startdAddr[32];
	char startdName[64];
	char remoteUser[128];
	char *capability;
	SafeSock startdSock;
	bool send_failed;
	int want_claiming = -1;

	strcpy(startdAddr, "<0.0.0.0:0>");
	strcpy(startdName,"unknown");
	offer->LookupString (ATTR_NAME, startdName);

	// these will succeed
	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);

	// see if offer supports claiming or not
	offer->LookupBool(ATTR_WANT_CLAIMING,want_claiming);
	// if offer says nothing, see if request says something
	if ( want_claiming == -1 ) {
		request.LookupBool(ATTR_WANT_CLAIMING,want_claiming);
	}

	// these should too, but may not
	if (!offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)		||
		!offer->LookupString (ATTR_NAME, startdName))
	{
		// fatal error if we need claiming
		if ( want_claiming ) {
			dprintf (D_ALWAYS, "      Could not lookup %s and %s\n", 
					ATTR_NAME, ATTR_STARTD_IP_ADDR);
			return MM_BAD_MATCH;
		}
	}

	// find the startd's capability from the private ad
	if ( want_claiming ) {
		if (!(capability = getCapability (startdName, startdAddr, startdPvtAds)))
		{
			dprintf(D_ALWAYS,"      %s has no capability\n", startdName);
			return MM_BAD_MATCH;
		}
	} else {
		// Claiming is *not* desired
		capability = "null";
	}
	
	// ---- real matchmaking protocol begins ----
	// 1.  contact the startd 
	if ( want_claiming ) {
		dprintf (D_FULLDEBUG, "      Connecting to startd %s at %s\n", 
					startdName, startdAddr); 

		startdSock.timeout (NegotiatorTimeout);
		if (!startdSock.connect (startdAddr, 0))
		{
			dprintf(D_ALWAYS,"      Could not connect to %s\n", startdAddr);
			return MM_BAD_MATCH;
		}

		DCStartd startd( startdAddr );
		startd.startCommand (MATCH_INFO, &startdSock);

		// 2.  pass the startd MATCH_INFO and capability string
		dprintf (D_FULLDEBUG, "      Sending MATCH_INFO/capability\n" );
		dprintf (D_FULLDEBUG, "      (Capability is \"%s\" )\n", capability);
		startdSock.encode();
		if ( !startdSock.put (capability) || !startdSock.end_of_message())
		{
			dprintf (D_ALWAYS,"      Could not send MATCH_INFO/capability to %s\n",
						startdName );
			dprintf (D_FULLDEBUG, "      (Capability is \"%s\")\n", capability );
			return MM_BAD_MATCH;
		}
	}	// end of if want_claiming

	// 3.  send the match and capability to the schedd
	sock->encode();
	send_failed = false;	

	if ( send_ad_to_schedd ) 
	{
		dprintf(D_FULLDEBUG,
			"      Sending PERMISSION, capability, startdAd to schedd\n");
		if (!sock->put(PERMISSION_AND_AD) 	|| 
			!sock->put(capability)	||
			!offer->put(*sock)		||	// send startd ad to schedd
			!sock->end_of_message())
				send_failed = true;
	} else {
		dprintf(D_FULLDEBUG,
			"      Sending PERMISSION with capability to schedd\n");
		if (!sock->put(PERMISSION) 	|| 
			!sock->put(capability)	||
			!sock->end_of_message())
				send_failed = true;
	}

	if ( send_failed )
	{
		dprintf (D_ALWAYS, "      Could not send PERMISSION\n" );
		dprintf( D_FULLDEBUG, "      (Capability is \"%s\")\n", capability);
		sockCache->invalidateSock( scheddAddr );
		return MM_ERROR;
	}

	if (offer->LookupString(ATTR_REMOTE_USER, remoteUser) == 0) {
		strcpy(remoteUser, "none");
	}
	if (offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr) == 0) {
		strcpy(startdAddr, "<0.0.0.0:0>");
	}
	dprintf(D_MATCH, "      Matched %d.%d %s %s preempting %s %s\n",
			cluster, proc, scheddName, scheddAddr, remoteUser,
			startdAddr);

#if WANT_NETMAN
	// match was successful; commit our network bandwidth allocation
	// (this will generate D_BANDWIDTH debug messages)
	netman.CommitPlacement(scheddName);
#endif

    // 4. notifiy the accountant
	dprintf(D_FULLDEBUG,"      Notifying the accountant\n");
	accountant.AddMatch(scheddName, offer);

	// done
	dprintf (D_ALWAYS, "      Successfully matched with %s\n", startdName);
	return MM_GOOD_MATCH;
}


void Matchmaker::
calculateNormalizationFactor (ClassAdList &scheddAds,
							  double &max, double &normalFactor,
							  double &maxAbs, double &normalAbsFactor)
{
	ClassAd *ad;
	char	scheddName[64];
	double	prio, prioFactor;
	char	old_scheddName[64];

	// find the maximum of the priority values (i.e., lowest priority)
	max = maxAbs = DBL_MIN;
	scheddAds.Open();
	while ((ad = scheddAds.Next()))
	{
		// this will succeed (comes from collector)
		ad->LookupString (ATTR_NAME, scheddName);
		prio = accountant.GetPriority (scheddName);
		if (prio > max) max = prio;
		prioFactor = accountant.GetPriorityFactor (scheddName);
		if (prioFactor > maxAbs) maxAbs = prioFactor;
	}
	scheddAds.Close();

	// calculate the normalization factor, i.e., sum of the (max/scheddprio)
	// also, do not factor in ads with the same ATTR_NAME more than once -
	// ads with the same ATTR_NAME signify the same user submitting from multiple
	// machines.
	normalFactor = 0.0;
	normalAbsFactor = 0.0;
	old_scheddName[0] = '\0';
	scheddAds.Open();
	while ((ad = scheddAds.Next()))
	{
		ad->LookupString (ATTR_NAME, scheddName);
		if ( strcmp(scheddName,old_scheddName) == 0) continue;
		strncpy(old_scheddName,scheddName,sizeof(old_scheddName));
		prio = accountant.GetPriority (scheddName);
		normalFactor = normalFactor + max/prio;
		prioFactor = accountant.GetPriorityFactor (scheddName);
		normalAbsFactor = normalAbsFactor + maxAbs/prioFactor;
	}
	scheddAds.Close();

	// done
	return;
}


char *Matchmaker::
getCapability (char *startdName, char *startdAddr, ClassAdList &startdPvtAds)
{
	ClassAd *pvtAd;
	static char	capability[80];
	char	name[64];
	char	addr[64];

	startdPvtAds.Open();
	while ((pvtAd = startdPvtAds.Next()))
	{
		if (pvtAd->LookupString (ATTR_NAME, name)			&&
			strcmp (name, startdName) == 0					&&
			pvtAd->LookupString (ATTR_STARTD_IP_ADDR, addr)	&&
			strcmp (addr, startdAddr) == 0					&&
			pvtAd->LookupString (ATTR_CAPABILITY, capability))
		{
			startdPvtAds.Close ();
			return capability;
		}
	}
	startdPvtAds.Close();
	return NULL;
}

void Matchmaker::
addRemoteUserPrios( ClassAdList &cal )
{
	ClassAd	*ad;
	char	remoteUser[64];
	char	buffer[128];
	float	prio;

	cal.Open();
	while( ( ad = cal.Next() ) ) {
		if( ad->LookupString( ATTR_REMOTE_USER , remoteUser ) ) {
			prio = (float) accountant.GetPriority( remoteUser );
			(void) sprintf( buffer , "%s = %f" , ATTR_REMOTE_USER_PRIO , prio );
			ad->Insert( buffer );
		}
	}
	cal.Close();
}

void Matchmaker::
reeval(ClassAd *ad) 
{
	int cur_matches;
	MapEntry *oldAdEntry = NULL;
	char    remoteHost[MAXHOSTNAMELEN];	
	char    buffer[255];
	
	cur_matches = 0;
	ad->EvalInteger("CurMatches", NULL, cur_matches);

	ad->LookupString(ATTR_NAME, remoteHost);
	MyString rhost(remoteHost);
	stashedAds->lookup( rhost, oldAdEntry);
		
	cur_matches++;
	snprintf(buffer, 255, "CurMatches = %d", cur_matches);
	ad->InsertOrUpdate(buffer);
	if(oldAdEntry) {
		delete(oldAdEntry->oldAd);
		oldAdEntry->oldAd = new ClassAd(*ad);
	}
}

int Matchmaker::HashFunc(const MyString &Key, int TableSize) {
	return Key.Hash() % TableSize;
}

Matchmaker::MatchListType::
MatchListType(int maxlen)
{
	AdListArray = new AdListEntry[maxlen];
	ASSERT(AdListArray);
	adListLen = 0;
	adListHead = 0;
	m_rejForNetwork = 0; 
	m_rejForNetworkShare = 0;
	m_rejPreemptForPrio = 0;
	m_rejPreemptForPolicy = 0; 
	m_rejPreemptForRank = 0;
}

Matchmaker::MatchListType::
~MatchListType()
{
	if (AdListArray) {
		delete [] AdListArray;
	}
}

ClassAd* Matchmaker::MatchListType::
pop_candidate()
{
	ClassAd* candidate = NULL;

	while ( adListHead < adListLen && !candidate ) {
		candidate = AdListArray[adListHead].ad;
		adListHead++;
	}

	return candidate;
}


void Matchmaker::MatchListType::
get_diagnostics(int & rejForNetwork,
					int & rejForNetworkShare,
					int & rejPreemptForPrio,
					int & rejPreemptForPolicy,
					int & rejPreemptForRank)
{
	rejForNetwork = m_rejForNetwork;
	rejForNetworkShare = m_rejForNetworkShare;
	rejPreemptForPrio = m_rejPreemptForPrio;
	rejPreemptForPolicy = m_rejPreemptForPolicy;
	rejPreemptForRank = m_rejPreemptForRank;
}

void Matchmaker::MatchListType::
set_diagnostics(int rejForNetwork,
					int rejForNetworkShare,
					int rejPreemptForPrio,
					int rejPreemptForPolicy,
					int rejPreemptForRank)
{
	m_rejForNetwork = rejForNetwork;
	m_rejForNetworkShare = rejForNetworkShare;
	m_rejPreemptForPrio = rejPreemptForPrio;
	m_rejPreemptForPolicy = rejPreemptForPolicy;
	m_rejPreemptForRank = rejPreemptForRank;
}

void Matchmaker::MatchListType::
add_candidate(ClassAd * candidate,
					double candidateRankValue,
					double candidatePreJobRankValue,
					double candidatePostJobRankValue,
					double candidatePreemptRankValue,
					PreemptState candidatePreemptState)
{
	ASSERT(AdListArray);
	
	AdListArray[adListLen].ad = candidate;
	AdListArray[adListLen].RankValue = candidateRankValue;
	AdListArray[adListLen].PreJobRankValue = candidatePreJobRankValue;
	AdListArray[adListLen].PostJobRankValue = candidatePostJobRankValue;
	AdListArray[adListLen].PreemptRankValue = candidatePreemptRankValue;
	AdListArray[adListLen].PreemptStateValue = candidatePreemptState;

	adListLen++;
}


int Matchmaker::
groupSortCompare(const void* elem1, const void* elem2)
{
	SimpleGroupEntry* Elem1 = (SimpleGroupEntry*) elem1;
	SimpleGroupEntry* Elem2 = (SimpleGroupEntry*) elem2;

	if ( Elem1->prio < Elem2->prio ) {
		return -1;
	} 
	if ( Elem1->prio == Elem2->prio ) {
		return 0;
	} 
	return 1;
}

int Matchmaker::MatchListType::
sort_compare(const void* elem1, const void* elem2)
{
	AdListEntry* Elem1 = (AdListEntry*) elem1;
	AdListEntry* Elem2 = (AdListEntry*) elem2;

	double			candidateRankValue = Elem1->RankValue;
	double			candidatePreJobRankValue = Elem1->PreJobRankValue;
	double			candidatePostJobRankValue = Elem1->PostJobRankValue;
	double			candidatePreemptRankValue = Elem1->PreemptRankValue;
	PreemptState	candidatePreemptState = Elem1->PreemptStateValue;

	double			bestRankValue = Elem2->RankValue;
	double			bestPreJobRankValue = Elem2->PreJobRankValue;
	double			bestPostJobRankValue = Elem2->PostJobRankValue;
	double			bestPreemptRankValue = Elem2->PreemptRankValue;
	PreemptState	bestPreemptState = Elem2->PreemptStateValue;

	if ( candidateRankValue == bestRankValue &&
		 candidatePreJobRankValue == bestPreJobRankValue &&
		 candidatePostJobRankValue == bestPostJobRankValue &&
		 candidatePreemptRankValue == bestPreemptRankValue &&
		 candidatePreemptState == bestPreemptState )
	{
		return 0;
	}

	// the quality of a match is determined by a lexicographic sort on
	// the following values, but more is better for each component
	//  1. negotiator pre job rank
	//  1. job rank of offer 
	//  2. negotiator post job rank
	//	3. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
	//  4. preemption rank (if preempting)

	bool newBestFound = false;

	if(candidatePreJobRankValue < bestPreJobRankValue);
	else if(candidatePreJobRankValue > bestPreJobRankValue) {
		newBestFound = true;
	}
	else if(candidateRankValue < bestRankValue);
	else if(candidateRankValue > bestRankValue) {
		newBestFound = true;
	}
	else if(candidatePostJobRankValue < bestPostJobRankValue);
	else if(candidatePostJobRankValue > bestPostJobRankValue) {
		newBestFound = true;
	}
	else if(candidatePreemptState < bestPreemptState);
	else if(candidatePreemptState > bestPreemptState) {
		newBestFound = true;
	}
	//NOTE: if NO_PREEMPTION, PreemptRank is a constant
	else if(candidatePreemptRankValue < bestPreemptRankValue);
	else if(candidatePreemptRankValue > bestPreemptRankValue) {
		newBestFound = true;
	}

	if ( newBestFound ) {
		// candidate is better: candidate is elem1, and qsort man page
		// says return < 0 is elem1 is less than elem2
		return -1;
	} else {
		return 1;
	}
}
			
void Matchmaker::MatchListType::
sort()
{
	// Note: since we must use static members, sort() is
	// _NOT_ thread safe!!!
	qsort(AdListArray,adListLen,sizeof(AdListEntry),sort_compare);
}
