/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include <math.h>
#include <float.h>
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "condor_api.h"

// the comparison function must be declared before the declaration of the
// matchmaker class in order to preserve its static-ness.  (otherwise, it
// is forced to be extern.)

static int comparisonFunction (ClassAd *, ClassAd *, void *);
#include "matchmaker.h"

// possible outcomes of negotiating with a schedd
enum { MM_ERROR, MM_DONE, MM_RESUME };

// possible outcomes of a matchmaking attempt
enum { _MM_ERROR, MM_NO_MATCH, MM_GOOD_MATCH, MM_BAD_MATCH };

typedef int (*lessThanFunc)(AttrListAbstract*, AttrListAbstract*, void*);

Matchmaker::
Matchmaker ()
{
	char buf[64];

	AccountantHost  = NULL;
	PreemptionReq = NULL;
	PreemptionRank = NULL;
	sockCache = NULL;

	sprintf (buf, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondStd);

	sprintf (buf, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondPrioPreempt);

	negotiation_timerID = -1;
	GotRescheduleCmd=false;
}


Matchmaker::
~Matchmaker()
{
	if (AccountantHost) free (AccountantHost);
	delete rankCondStd;
	delete rankCondPrioPreempt;
	delete PreemptionReq;
	delete PreemptionRank;
	delete sockCache;
}


void Matchmaker::
initialize ()
{
	// read in params
	reinitialize ();

    // register commands
    daemonCore->Register_Command (RESCHEDULE, "Reschedule", 
            (CommandHandlercpp) &Matchmaker::RESCHEDULE_commandHandler, 
			"RESCHEDULE_commandHandler", (Service*) this, WRITE);
    daemonCore->Register_Command (RESET_ALL_USAGE, "ResetAllUsage",
            (CommandHandlercpp) &Matchmaker::RESET_ALL_USAGE_commandHandler, 
			"RESET_ALL_USAGE_commandHandler", this, ADMINISTRATOR);
    daemonCore->Register_Command (RESET_USAGE, "ResetUsage",
            (CommandHandlercpp) &Matchmaker::RESET_USAGE_commandHandler, 
			"RESET_USAGE_commandHandler", this, ADMINISTRATOR);
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

	// Set a timer to renegotiate.
    negotiation_timerID = daemonCore->Register_Timer (0,  NegotiatorInterval,
			(TimerHandlercpp) &Matchmaker::negotiationTime, 
			"Time to negotiate", this);

}


int Matchmaker::
reinitialize ()
{
	char *tmp;
	int TrafficInterval=0;
	double TrafficLimit = 0.0;

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

	tmp = param("NEGOTIATOR_TRAFFIC_LIMIT");
	if( tmp ) {
		TrafficLimit = atof(tmp);
		free( tmp );
	}

	tmp = param("NEGOTIATOR_TRAFFIC_INTERVAL");
	if( tmp ) {
		TrafficInterval = atoi(tmp);
		free( tmp );
	}

	NetUsage.SetMax(TrafficLimit, TrafficInterval);

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
	dprintf (D_ALWAYS,"NEGOTIATOR_TRAFFIC_LIMIT = %f kbytes\n",TrafficLimit);
	dprintf (D_ALWAYS,"NEGOTIATOR_TRAFFIC_INTERVAL = %d sec\n",
			 TrafficInterval);
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

	// done
	return TRUE;
}


int Matchmaker::
RESCHEDULE_commandHandler (int, Stream *)
{
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
		dprintf (D_ALWAYS, "Could not read eom\n");
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


int Matchmaker::
negotiationTime ()
{
	ClassAdList startdAds;			// 1. get from collector
	ClassAdList startdPvtAds;		// 2. get from collector
	ClassAdList scheddAds;			// 3. get from collector
	ClassAdList ClaimedStartdAds;   // 4. get from collector

	ClassAd		*schedd;
	char		scheddName[80];
	char		scheddAddr[32];
	int			result;
	int			numStartdAds;
	double		maxPrioValue;
	double		normalFactor;
	double		scheddPrio;
	double		scheddShare;
	int			scheddLimit;
	int			scheddUsage;
	int			MaxscheddLimit;
	int			hit_schedd_prio_limit;
	int 		send_ad_to_schedd;	
	static time_t completedLastCycleTime = 0;

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

	// ----- Get all required ads from the collector
	dprintf( D_ALWAYS, "Phase 1:  Obtaining ads from collector ...\n" );
	if( !obtainAdsFromCollector( startdAds, scheddAds, startdPvtAds, 
		ClaimedStartdAds ) )
	{
		dprintf( D_ALWAYS, "Aborting negotiation cycle\n" );
		// should send email here
		return FALSE;
	}

	// ----- Recalculate priorities for schedds
	dprintf( D_ALWAYS, "Phase 2:  Performing accounting ...\n" );
	accountant.UpdatePriorities();
	accountant.CheckMatches( ClaimedStartdAds );
	// for ads which have RemoteUser set, add RemoteUserPrio
	addRemoteUserPrios( startdAds ); 

	// ----- Sort the schedd list in decreasing priority order
	dprintf( D_ALWAYS, "Phase 3:  Sorting schedd ads by priority ...\n" );
	scheddAds.Sort( (lessThanFunc)comparisonFunction, this );

	int spin_pie=0;
	do {
		spin_pie++;
		hit_schedd_prio_limit = FALSE;
		calculateNormalizationFactor( scheddAds, maxPrioValue, normalFactor);
		numStartdAds = startdAds.MyLength();
		MaxscheddLimit = 0;
		// ----- Negotiate with the schedds in the sorted list
		dprintf( D_ALWAYS, "Phase 4.%d:  Negotiating with schedds ...\n",
			spin_pie );
		dprintf (D_FULLDEBUG, "    NumStartdAds = %d\n", numStartdAds);
		dprintf (D_FULLDEBUG, "    NormalFactor = %f\n", normalFactor);
		dprintf (D_FULLDEBUG, "    MaxPrioValue = %f\n", maxPrioValue);
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
			dprintf(D_ALWAYS,"  Negotiating with %s at %s\n",scheddName,
				scheddAddr);

			// should we send the startd ad to this schedd?
			send_ad_to_schedd = FALSE;
			schedd->LookupBool( ATTR_WANT_RESOURCE_AD, send_ad_to_schedd);
	
			// calculate the percentage of machines that this schedd can use
			scheddPrio = accountant.GetPriority ( scheddName );
			scheddUsage = accountant.GetResourcesUsed ( scheddName );
			scheddShare = maxPrioValue/(scheddPrio*normalFactor);
			scheddLimit  = (int) rint((scheddShare*numStartdAds)-scheddUsage);
			if (scheddLimit>MaxscheddLimit) MaxscheddLimit=scheddLimit;

			// print some debug info
			dprintf (D_FULLDEBUG, "  Calculating schedd limit with the "
				"following parameters\n");
			dprintf (D_FULLDEBUG, "    ScheddPrio     = %f\n", scheddPrio);
			dprintf (D_FULLDEBUG, "    scheddShare    = %f\n", scheddShare);
			dprintf (D_FULLDEBUG, "    ScheddUsage    = %d\n", scheddUsage);
			dprintf (D_FULLDEBUG, "    scheddLimit    = %d\n", scheddLimit);
			dprintf (D_FULLDEBUG, "    MaxscheddLimit = %d\n", MaxscheddLimit);
		
			if ( scheddLimit < 1 ) {
				// Optimization: If limit is 0, don't waste time with negotiate
				result = MM_RESUME;
			} else {
				result=negotiate( scheddName,scheddAddr,scheddPrio,scheddLimit, 
							startdAds, startdPvtAds, send_ad_to_schedd);
			}

			switch (result)
			{
				case MM_RESUME:
					// the schedd hit its resource limit.  must resume 
					// negotiations at a later negotiation cycle.
					dprintf(D_FULLDEBUG,"  This schedd hit its scheddlimit.\n");
					hit_schedd_prio_limit = TRUE;
					break;
				case MM_DONE: 
					// the schedd got all the resources it wanted. delete this 
					// schedd ad.
					dprintf(D_FULLDEBUG,"  This schedd got all it wants; "
						"removing it.\n");
					scheddAds.Delete( schedd);
					break;
				case MM_ERROR:
				default:
					dprintf(D_ALWAYS,"  Error: Ignoring schedd for this cycle\n" );
					scheddAds.Delete( schedd );
			}
		}
		scheddAds.Close();
	} while ( hit_schedd_prio_limit == TRUE && MaxscheddLimit>0 );

	// ----- Done with the negotiation cycle
	dprintf( D_ALWAYS, "---------- Finished Negotiation Cycle ----------\n" );

	completedLastCycleTime = time(NULL);

	return TRUE;
}


static int
comparisonFunction (ClassAd *ad1, ClassAd *ad2, void *m)
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

	if (prio1==prio2) return (strcmp(scheddName1,scheddName2) < 0);
	return (prio1 < prio2);
}



bool Matchmaker::
obtainAdsFromCollector (ClassAdList &startdAds, 
						ClassAdList &scheddAds, 
						ClassAdList &startdPvtAds,
						ClassAdList &ClaimedStartdAds)
{
	CondorQuery startdQuery    (STARTD_AD);
	CondorQuery scheddQuery    (SUBMITTOR_AD);
	CondorQuery startdPvtQuery (STARTD_PVT_AD);
	CondorQuery ClaimedStartdQuery    (STARTD_AD);
	QueryResult result;
	char buffer[1024];

	// set the constraints on the various queries
	// 1.  Fetch ads of startds that are CLAIMED or UNCLAIMED
	dprintf (D_ALWAYS, "  Getting startd ads ...\n");
	if( (result = startdQuery.fetchAds(startdAds)) != Q_OK )
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch startd ads ... aborting\n",
			getStrQueryResult(result));
		return false;
	}

	// 2.  Only obtain schedd ads that have idle jobs
	dprintf (D_ALWAYS, "  Getting schedd ads ...\n");
	sprintf (buffer, "%s > 0", ATTR_IDLE_JOBS);
	if (((result = scheddQuery.addANDConstraint (buffer)) != Q_OK) ||
		((result = scheddQuery.fetchAds(scheddAds))    != Q_OK))
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch schedd ads ... aborting\n",
			getStrQueryResult(result));
		return false;
	}

	// 3. (no constraint on private ads)	
	dprintf (D_ALWAYS, "  Getting startd private ads ...\n");
	if	((result = startdPvtQuery.fetchAds(startdPvtAds)) != Q_OK)
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch startd private ads ... aborting\n",
			getStrQueryResult(result));
		return false;
	}

	// set the constraints on the various queries
	// 4.  Fetch ads of startds that are CLAIMED
	dprintf (D_ALWAYS, "  Getting startd ads ...\n");
	sprintf (buffer,"(%s == \"%s\")",ATTR_STATE,state_to_string(claimed_state));
	if (((result = ClaimedStartdQuery.addANDConstraint(buffer))	!= Q_OK) ||
		((result = ClaimedStartdQuery.fetchAds(ClaimedStartdAds))!= Q_OK))
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch startd ads ... aborting\n",
			getStrQueryResult(result));
		return false;
	}

	dprintf (D_ALWAYS, "Got ads: %d startd ; %d schedd ; %d startd private ; "
		"%d claimed startd\n", startdAds.MyLength(), scheddAds.MyLength(), 
		startdPvtAds.MyLength(),ClaimedStartdAds.MyLength());

	return true;
}


int Matchmaker::
negotiate (char *scheddName, char *scheddAddr, double priority, int scheddLimit,
	ClassAdList &startdAds, ClassAdList &startdPvtAds, int send_ad_to_schedd)
{
	ReliSock	*sock;
	int			i;
	int			reply;
	int			cluster, proc;
	int			placement_bw, preempt_bw, bw_request, job_universe;
	int			result;
	ClassAd		request;
	ClassAd		*offer;
	char		prioExpr[128], startdAddr[32], remoteUser[128];

	// 0.  connect to the schedd --- ask the cache for a connection
	if (!sockCache->getReliSock((Sock *&)sock, scheddAddr, NegotiatorTimeout))
	{
		dprintf (D_ALWAYS, "    Failed to connect to %s\n", scheddAddr);
		return MM_ERROR;
	}

	// 1.  send NEGOTIATE command, followed by the scheddName (user@uiddomain)
	sock->encode();
	if (!sock->put(NEGOTIATE)||!sock->put(scheddName)||!sock->end_of_message())
	{
		dprintf (D_ALWAYS, "    Failed to send NEGOTIATE/scheddName/eom\n");
		sockCache->invalidateSock(scheddAddr);
		return MM_ERROR;
	}

	// setup expression with the submittor's priority
	(void) sprintf( prioExpr , "%s = %f" , ATTR_SUBMITTOR_PRIO , priority );

	// 2.  negotiation loop with schedd
	for (i = 0; i < scheddLimit;  i++)
	{
		// Service any interactive commands on our command socket.
		// This keeps condor_userprio hanging to a minimum when
		// we are involved in a lot of schedd negotiating.
		// It also performs the important function of draining out
		// any reschedule requests queued up on our command socket, so
		// we do not negotiate over & over unnecesarily.
		daemonCore->ServiceCommandSocket();

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
			return MM_DONE;
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
			if (!(offer=matchmakingAlgorithm(scheddName, request, startdAds,
											 priority)))
			{
				// no preemptable resource offer either ... 
				dprintf(D_ALWAYS,
						"      Rejected\n");
				sock->encode();
				if (!sock->put(REJECTED) || !sock->end_of_message())
					{
						dprintf (D_ALWAYS, "      Could not send rejection\n");
						sock->end_of_message ();
						sockCache->invalidateSock(scheddAddr);
						
						return MM_ERROR;
					}
				result = MM_NO_MATCH;
				continue;
			}
			else
			{
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
			}

			// Make sure this offer won't put us over our network bandwidth
			// limit.
			if (!request.LookupInteger (ATTR_DISK_USAGE,placement_bw)) {
				if (!request.LookupInteger (ATTR_EXECUTABLE_SIZE,
											placement_bw)) {
					placement_bw = 0;
				}
			}
			if (!request.LookupInteger (ATTR_JOB_UNIVERSE, job_universe)) {
				job_universe = STANDARD; // err on the safe side
			}
			if (job_universe == STANDARD) {
				float cpu_time;
				if (!request.LookupFloat (ATTR_JOB_REMOTE_USER_CPU,
										  cpu_time)) {
					cpu_time = 1.0;	// err on the safe side
				}
				if (cpu_time > 0.0) {
					// if job_universe is STANDARD (checkpointing is
					// enabled) and cpu_time > 0.0 (job has committed
					// some work), then the job will need to read a
					// checkpoint to restart, so we set the placement
					// cost to be the image size, which includes the
					// executable size.
					request.LookupInteger (ATTR_IMAGE_SIZE, placement_bw);
				}
			}
			if (!offer->LookupInteger (ATTR_JOB_UNIVERSE, job_universe)) {
				job_universe = STANDARD; // err on the safe side
			}
			if (job_universe == STANDARD) {
				// if preempted job is a STANDARD universe job, it will
				// need to write a checkpoint, so we include image size
				// in the preemption cost
				if (offer->LookupInteger (ATTR_IMAGE_SIZE, preempt_bw) == 0) {
					preempt_bw = 0;
				}
			} else {
				preempt_bw = 0;
			}
			if (offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr) == 0) {
				strcpy(startdAddr, "<0.0.0.0:0>");
			}
			bw_request = NetUsage.Request(placement_bw+preempt_bw);
			dprintf( D_BANDWIDTH,
					 "    %d+%d KB for %d.%d %s %s preempting %s %s %s\n",
					 placement_bw, preempt_bw, cluster, proc, scheddName,
					 scheddAddr, remoteUser, startdAddr, 
					 (bw_request > 0) ? "DENIED" : "GRANTED");
			if (bw_request > 0) { // reject match -- over bw limit
				dprintf( D_ALWAYS, "    Not enough bandwidth for this job ---"
						 " rejecting.\n" );
				sock->encode();
				if (!sock->put(REJECTED) || !sock->end_of_message())
				{
					dprintf (D_ALWAYS, "      Could not send rejection\n");
					sock->end_of_message ();
					sockCache->invalidateSock(scheddAddr);
					return MM_ERROR;
				}
				result = MM_NO_MATCH;
				continue;
			}

			// 2e(ii).  perform the matchmaking protocol
			result = matchmakingProtocol (request, offer, startdPvtAds, sock, 
					scheddName, send_ad_to_schedd);

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
		startdAds.Delete (offer);
	}

	// 3.  check if we hit our resource limit
	if (i == scheddLimit)
	{
		dprintf (D_ALWAYS, 	
				"    Reached schedd resource limit: %d ... stopping\n", i);
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


ClassAd *Matchmaker::
matchmakingAlgorithm(char *, ClassAd &request,ClassAdList &startdAds,
			double preemptPrio)
{
		// the order of values in this enumeration is important!
	enum PreemptState {PRIO_PREEMPTION,RANK_PREEMPTION,NO_PREEMPTION};
		// to store values pertaining to a particular candidate offer
	ClassAd 		*candidate;
	double			candidateRankValue;
	double			candidatePreemptRankValue;
	PreemptState	candidatePreemptState;
		// to store the best candidate so far
	ClassAd 		*bestSoFar = NULL;	
	double			bestRankValue = -(FLT_MAX);
	double			bestPreemptRankValue = -(FLT_MAX);
	PreemptState	bestPreemptState = (PreemptState)-1;
	bool			newBestFound;
		// to store results of evaluations
	char			remoteUser[128];
	EvalResult		result;
	float			tmp;


	// scan the offer ads
	startdAds.Open ();
	while ((candidate = startdAds.Next ())) {
		// the candidate offer and request must match
		if( !( *candidate == request ) ) {
				// they don't match; continue
			continue;
		}

		candidatePreemptState = NO_PREEMPTION;
		// if there is a remote user, consider preemption ....
		if (candidate->LookupString (ATTR_REMOTE_USER, remoteUser) ) {
				// check if we are preempting for rank or priority
			if( rankCondStd->EvalTree( candidate, &request, &result ) && 
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
					continue;
				}
					// (2) we need to make sure that the machine ranks the job
					// at least as well as the one it is currently running 
					// (i.e., rankCondPrioPreempt holds)
				if(!(rankCondPrioPreempt->EvalTree(candidate,&request,&result)&&
						result.type == LX_INTEGER && result.i == TRUE ) ) {
						// machine doesn't like this job as much -- find another
					continue;
				}
			} else {
					// don't have better priority *and* offer doesn't prefer
					// request --- find another machine
				continue;
			}
		}


		// calculate the request's rank of the offer
		if(!request.EvalFloat(ATTR_RANK,candidate,tmp)) {
			tmp = 0.0;
		}
		candidateRankValue = tmp;

		// the quality of a match is determined by a lexicographic sort on
		// the following values, but more is better for each component
		//  1. job rank of offer 
		//	2. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
		//  3. preemption rank (if preempting)
		newBestFound = false;
		candidatePreemptRankValue = -(FLT_MAX);
		if( candidatePreemptState != NO_PREEMPTION ) {
			// calculate the preemption rank
			if( PreemptionRank &&
			   		PreemptionRank->EvalTree(candidate,&request,&result) &&
					result.type == LX_FLOAT) {
				candidatePreemptRankValue = result.f;
			} else if( PreemptionRank ) {
				dprintf(D_ALWAYS, "Failed to evaluate PREEMPTION_RANK "
					"expression to a float.\n");
			}
		}
		if( ( candidateRankValue > bestRankValue ) || 	// first by job rank
				( candidateRankValue==bestRankValue && 	// then by preempt state
				candidatePreemptState > bestPreemptState ) ) {
			newBestFound = true;
		} else if( candidateRankValue==bestRankValue && // then by preempt rank
				candidatePreemptState==bestPreemptState && 
				bestPreemptState != NO_PREEMPTION ) {
				// check if the preemption rank is better than the best
			if( candidatePreemptRankValue > bestPreemptRankValue ) {
				newBestFound = true;
			}
		} 

		if( newBestFound ) {
			bestSoFar = candidate;
			bestRankValue = candidateRankValue;
			bestPreemptState = candidatePreemptState;
			bestPreemptRankValue = candidatePreemptRankValue;
		}
	}
	startdAds.Close ();


	// this is the best match
	return bestSoFar;
}


int Matchmaker::
matchmakingProtocol (ClassAd &request, ClassAd *offer, 
						ClassAdList &startdPvtAds, Sock *sock, char* scheddName,
						int send_ad_to_schedd)
{
	int  cluster, proc;
	char startdAddr[32];
	char startdName[64];
	char *capability;
	SafeSock startdSock;
	bool send_failed;

	// these will succeed
	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);

	// these should too, but may not
	if (!offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)		||
		!offer->LookupString (ATTR_NAME, startdName))
	{
		dprintf (D_ALWAYS, "      Could not lookup %s and %s\n", 
					ATTR_NAME, ATTR_STARTD_IP_ADDR);
		return MM_BAD_MATCH;
	}

	// find the startd's capability from the private ad
	if (!(capability = getCapability (startdName, startdAddr, startdPvtAds)))
	{
		dprintf(D_ALWAYS,"      %s has no capability\n", startdName);
		return MM_BAD_MATCH;
	}
	
	// ---- real matchmaking protocol begins ----
	// 1.  contact the startd 
	dprintf (D_FULLDEBUG, "      Connecting to startd %s at %s\n", 
				startdName, startdAddr); 
	startdSock.timeout (NegotiatorTimeout);
	if (!startdSock.connect (startdAddr, 0))
	{
		dprintf(D_ALWAYS,"      Could not connect to %s\n", startdAddr);
		return MM_BAD_MATCH;
	}

	// 2.  pass the startd MATCH_INFO and capability string
	dprintf (D_FULLDEBUG, "      Sending MATCH_INFO/capability\n" );
	dprintf (D_FULLDEBUG, "      (Capability is \"%s\" )\n", capability);
	startdSock.encode();
	if (!startdSock.put (MATCH_INFO) || 
		!startdSock.put (capability) || 
		!startdSock.end_of_message())
	{
		dprintf (D_ALWAYS,"      Could not send MATCH_INFO/capability to %s\n",
					startdName );
		dprintf (D_FULLDEBUG, "      (Capability is \"%s\")\n", capability );
		return MM_BAD_MATCH;
	}

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
		return MM_ERROR;
	}

    // 4. notifiy the accountant
	dprintf(D_FULLDEBUG,"      Notifying the accountant\n");
	accountant.AddMatch(scheddName, offer);

	// done
	dprintf (D_ALWAYS, "      Successfully matched with %s\n", startdName);
	return MM_GOOD_MATCH;
}


void Matchmaker::
calculateNormalizationFactor (ClassAdList &scheddAds, double &max, 
				double &normalFactor)
{
	ClassAd *ad;
	char	scheddName[64];
	double	prio;
	char	old_scheddName[64];
	int 	num_scheddAds;

	// find the maximum of the priority values (i.e., lowest priority)
	max = DBL_MIN;
	scheddAds.Open();
	while ((ad = scheddAds.Next()))
	{
		// this will succeed (comes from collector)
		ad->LookupString (ATTR_NAME, scheddName);
		prio = accountant.GetPriority (scheddName);
		if (prio > max) max = prio;
	}
	scheddAds.Close();

	// calculate the normalization factor, i.e., sum of the (max/scheddprio)
	// also, do not factor in ads with the same ATTR_NAME more than once -
	// ads with the same ATTR_NAME signify the same user submitting from multiple
	// machines.  count the number of ads with unique ATTR_NAME's into 
	// the num_scheddsAds paramenter.
	normalFactor = 0.0;
	num_scheddAds = 0;	
	old_scheddName[0] = '\0';
	scheddAds.Open();
	while ((ad = scheddAds.Next()))
	{
		ad->LookupString (ATTR_NAME, scheddName);
		if ( strcmp(scheddName,old_scheddName) == 0) continue;
		strncpy(old_scheddName,scheddName,sizeof(old_scheddName));
		num_scheddAds++;
		prio = accountant.GetPriority (scheddName);
		normalFactor = normalFactor + max/prio;
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
