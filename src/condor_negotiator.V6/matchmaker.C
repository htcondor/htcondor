#include <math.h>
#include <float.h>
#include "condor_common.h"
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
	PreemptionHold = NULL;

	sprintf (buf, "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondStd);

	sprintf (buf, "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	Parse (buf, rankCondPreempt);
}


Matchmaker::
~Matchmaker()
{
	if (AccountantHost) free (AccountantHost);
	delete rankCondStd;
	delete rankCondPreempt;
	delete PreemptionHold;
}


void Matchmaker::
initialize ()
{
	// read in params
	reinitialize ();

    // register commands
    daemonCore->Register_Command (RESCHEDULE, "Reschedule", 
            (CommandHandlercpp) &RESCHEDULE_commandHandler, 
			"RESCHEDULE_commandHandler", (Service*) this);
    daemonCore->Register_Command (SET_PRIORITY, "SetPriority",
            (CommandHandlercpp) &SET_PRIORITY_commandHandler, 
			"SET_PRIORITY_commandHandler", this, NEGOTIATOR);

    // ensure that we renegotiate at least once every 'NegtiatorInterval'
    daemonCore->Register_Timer (0, NegotiatorInterval, 
			(TimerHandlercpp)negotiationTime, "Time to negotiate", this);
}


int Matchmaker::
reinitialize ()
{
	char *tmp;

	// get timeout values
	NegotiatorInterval 	= (tmp=param("NEGOTIATOR_INTERVAL")) ? atoi(tmp): 300;
	NegotiatorTimeout  	= (tmp=param("NEGOTIATOR_TIMEOUT"))  ? atoi(tmp): 30;

	// get PreemptionHold expression
	if (PreemptionHold) delete PreemptionHold;
	PreemptionHold = NULL;
	if (tmp && Parse(tmp, PreemptionHold))
	{
		EXCEPT ("Error parsing PREEMPTION_GUARD expression: %s", tmp);
	}

	dprintf (D_ALWAYS,"ACCOUNTANT_HOST     = %s\n", AccountantHost ? 
			AccountantHost : "None (local)");
	dprintf (D_ALWAYS,"NEGOTIATOR_INTERVAL = %d sec\n",NegotiatorInterval);
	dprintf (D_ALWAYS,"NEGOTIATOR_TIMEOUT  = %d sec\n",NegotiatorTimeout);
	dprintf (D_ALWAYS,"PREEMPTION_HOLD     = %s\n", (tmp?tmp:"None"));

	// done
	return TRUE;
}


int Matchmaker::
negotiationTime ()
{
	return RESCHEDULE_commandHandler (0, NULL);
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
RESCHEDULE_commandHandler (int, Stream *)
{
	ClassAdList startdAds;			// 1. get from collector
	ClassAdList startdPvtAds;		// 2. get from collector
	ClassAdList scheddAds;			// 3. get from collector
	ClassAdList consumedStartds;	// 4. removed from (1) on match
	ClassAd		*schedd;
	char		scheddName[80];
	char		scheddAddr[32];
	char		startdAddr[32];
	int			result;
	int			numStartdAds;
	float		maxPrioValue;
	float		normalFactor;
	float		scheddPrio;
	int			scheddLimit;


	dprintf( D_ALWAYS, "---------- Started Negotiation Cycle ----------\n" );

	// ----- Get all required ads from the collector
	dprintf( D_ALWAYS, "Phase 1:  Obtaining ads from collector ...\n" );
	if( !obtainAdsFromCollector( startdAds, scheddAds, startdPvtAds ) )
	{
		dprintf( D_ALWAYS, "Aborting negotiation cycle\n" );
		// should send email here
		return FALSE;
	}

	// ----- Recalculate priorities for schedds
	dprintf( D_ALWAYS, "Phase 2:  Performing accounting ...\n" );
	accountant.UpdatePriorities();
	accountant.CheckMatches( startdAds );

	// ----- Sort the schedd list in decreasing priority order
	dprintf( D_ALWAYS, "Phase 3:  Sorting schedd ads by priority ...\n" );
	calculateNormalizationFactor( scheddAds, maxPrioValue, normalFactor );
	scheddAds.Sort( (lessThanFunc)comparisonFunction, this );

	// ----- Negotiate with the schedds in the sorted list
	dprintf( D_ALWAYS, "Phase 4:  Negotiating with schedds ...\n" );
	scheddAds.Open();
	while( schedd = scheddAds.Next())
	{
		// get the name and address of the schedd
		if( !schedd->LookupString( ATTR_NAME, scheddName ) ||
			!schedd->LookupString( ATTR_SCHEDD_IP_ADDR, scheddAddr ) )
		{
			dprintf (D_ALWAYS, "\tError!  Could not get %s and %s from ad\n",
						ATTR_NAME, ATTR_SCHEDD_IP_ADDR);
			return FALSE;
		}	
		dprintf(D_ALWAYS,"\tNegotiating with %s at %s\n",scheddName,scheddAddr);

		// calculate the percentage of machines that this schedd can use
		scheddPrio   = accountant.GetPriority ( scheddName );
		numStartdAds = startdAds.MyLength();

		dprintf (D_FULLDEBUG, "\tCalculating schedd limit with the following "
					"parameters\n");
		dprintf (D_FULLDEBUG, "\t\tMaxPrioValue = %f\n", maxPrioValue);
		dprintf (D_FULLDEBUG, "\t\tScheddPrio   = %f\n", scheddPrio);
		dprintf (D_FULLDEBUG, "\t\tNormalFactor = %f\n", normalFactor);
		dprintf (D_FULLDEBUG, "\t\tNumStartdAds = %d\n", numStartdAds);

		scheddLimit  = (int) ceil (maxPrioValue/(scheddPrio*normalFactor) *
									numStartdAds);

		dprintf(D_ALWAYS,"\tSchedd %s's resource limit set at %d\n",
				scheddName,scheddLimit);
		result = negotiate( scheddName, scheddAddr, scheddPrio, scheddLimit, 
						startdAds, startdPvtAds, consumedStartds);
		switch (result)
		{
			case MM_DONE: // the schedd got all the resources it wanted
			case MM_RESUME:
				// the schedd hit its resource limit.  must resume negotiations
				// at a later negotiation cycle; in either case adjust 
				// normalization factor
				normalFactor = normalFactor - (maxPrioValue / scheddPrio);
				break;

			case MM_ERROR:
			default:
				dprintf( D_ALWAYS,"\tError: Ignoring schedd for this cycle\n" );
				scheddAds.Delete( schedd );
		}
	}
	scheddAds.Close();

	// ----- Done with the negotiation cycle
	dprintf( D_ALWAYS, "---------- Finished Negotiation Cycle ----------\n" );
	return TRUE;
}


static int
comparisonFunction (ClassAd *ad1, ClassAd *ad2, void *m)
{
	char	scheddName1[64];
	char	scheddName2[64];
	float	prio1, prio2;
	Matchmaker *mm = (Matchmaker *) m;

	if (!ad1->LookupString (ATTR_NAME, scheddName1) ||
		!ad2->LookupString (ATTR_NAME, scheddName2))
	{
		return -1;
	}

	prio1 = mm->accountant.GetPriority(scheddName1);
	prio2 = mm->accountant.GetPriority(scheddName2);
	
	return (prio1 > prio2);
}



bool Matchmaker::
obtainAdsFromCollector (ClassAdList &startdAds, 
						ClassAdList &scheddAds, 
						ClassAdList &startdPvtAds)
{
	CondorQuery startdQuery    (STARTD_AD);
	CondorQuery scheddQuery    (SCHEDD_AD);
	CondorQuery startdPvtQuery (STARTD_PVT_AD);
	QueryResult result;
	char buffer[1024];

	// set the constraints on the various queries
	// 1.  Only obtain startd ads that may be willing to accept jobs
	dprintf (D_ALWAYS, "\tGetting startd ads ...\n");
	sprintf (buffer, "TARGET.%s != FALSE", ATTR_REQUIREMENTS);
	if (((result = startdQuery.addConstraint (buffer)) != Q_OK) ||
		((result = startdQuery.fetchAds(startdAds))    != Q_OK))
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch startd ads ... aborting\n",
			StrQueryResult[result]);
		return false;
	}

	// 2.  Only obtain schedd ads that have idle jobs
	dprintf (D_ALWAYS, "\tGetting schedd ads ...\n");
	sprintf (buffer, "%s > 0", ATTR_IDLE_JOBS);
	if (((result = scheddQuery.addConstraint (buffer)) != Q_OK) ||
		((result = scheddQuery.fetchAds(scheddAds))    != Q_OK))
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch schedd ads ... aborting\n",
			StrQueryResult[result]);
		return false;
	}

	// 3. (no constraint on private ads)	
	dprintf (D_ALWAYS, "\tGetting startd private ads ...\n");
	if	((result = startdPvtQuery.fetchAds(startdPvtAds)) != Q_OK)
	{
		dprintf (D_ALWAYS, 
			"Error %s:  failed to fetch startd private ads ... aborting\n",
			StrQueryResult[result]);
		return false;
	}

	dprintf (D_ALWAYS, "Got ads: %d startd ; %d schedd ; %d startd private\n",
		startdAds.MyLength(), scheddAds.MyLength(), startdPvtAds.MyLength());

	return true;
}


int Matchmaker::
negotiate (char *scheddName, char *scheddAddr, float priority, int scheddLimit,
			ClassAdList &startdAds, ClassAdList &startdPvtAds, 
			ClassAdList &consumedStartds)
{
	ReliSock	sock;
	int			i;
	int			reply;
	int			scheddPrio;
	int			cluster, proc;
	int			result;
	ClassAd		request;
	ClassAd		*offer;

	// 0.  connect to the schedd
	sock.timeout (NegotiatorTimeout);
	if (!sock.connect (scheddAddr, 0))
	{
		dprintf (D_ALWAYS, "\t\tFailed to connect to %s\n", scheddAddr);
		return MM_ERROR;
	}

	// 1.  send NEGOTIATE command, followed by the scheddName (user@uiddomain)
	sock.encode();
	if (!sock.put(NEGOTIATE) || !sock.put(scheddName) || !sock.end_of_message())
	{
		dprintf (D_ALWAYS, "\t\tFailed to send NEGOTIATE/scheddName/eom\n");
		return MM_ERROR;
	}

	// 2.  negotiation loop with schedd
	for (i = 0; i < scheddLimit;  i++)
	{
		// 2a.  ask for job information
		dprintf (D_FULLDEBUG, "\t\tSending SEND_JOB_INFO/eom\n");
		sock.encode();
		if (!sock.put(SEND_JOB_INFO) || !sock.end_of_message())
		{
			dprintf (D_ALWAYS, "\t\tFailed to send SEND_JOB_INFO/eom\n");
			return MM_ERROR;
		}

		// 2b.  the schedd may either reply with JOB_INFO or NO_MORE_JOBS
		dprintf (D_FULLDEBUG, "\t\tGetting reply from schedd ...\n");
		sock.decode();
		if (!sock.get (reply))
		{
			dprintf (D_ALWAYS, "\t\tFailed to get reply from schedd\n");
			sock.end_of_message ();
			return MM_ERROR;
		}

		// 2c.  if the schedd replied with NO_MORE_JOBS, cleanup and quit
		if (reply == NO_MORE_JOBS)
		{
			dprintf (D_ALWAYS, "\t\tGot NO_MORE_JOBS;  done negotiating\n");
			sock.end_of_message ();
			return MM_DONE;
		}
		else
		if (reply != JOB_INFO)
		{
			// something goofy
			dprintf(D_ALWAYS,"\t\tGot illegal command %d from schedd\n",reply);
			sock.end_of_message ();
			return MM_ERROR;
		}

		// 2d.  get the request 
		dprintf (D_FULLDEBUG,"\t\tGot JOB_INFO command; getting classad/eom\n");
		if (!request.get (sock) || !sock.end_of_message ())
		{
			dprintf(D_ALWAYS, "\t\tJOB_INFO command not followed by ad/eom\n");
			sock.end_of_message();
			return MM_ERROR;
		}
		if (!request.LookupInteger (ATTR_CLUSTER_ID, cluster) ||
			!request.LookupInteger (ATTR_PROC_ID, proc))
		{
			dprintf (D_ALWAYS, "\t\tCould not get %s and %s from request\n",
					ATTR_CLUSTER_ID, ATTR_PROC_ID);
			return MM_ERROR;
		}
		dprintf(D_ALWAYS, "\t\tRequest %05d.%05d:\n", cluster, proc);


		// 2e.  find a compatible offer for the request --- keep attempting
		//		to find matches until we can successfully (1) find a match,
		//		AND (2) notify the startd; so quit if we got a MM_GOOD_MATCH,
		//		or if MM_NO_MATCH could be found
		result = MM_BAD_MATCH;
		while (result == MM_BAD_MATCH) 
		{
			// 2e(i).  find a compatible offer
			if (!(offer = matchmakingAlgorithm (request, startdAds)))
			{
				// no offer ...
				dprintf(D_ALWAYS, "\t\t\tNo offer --- trying preemption\n");
			
				// try matchmaking algorithm with preemption mode enabled
				// (i.e., with priority == priority threshold)
				if (!(offer=matchmakingAlgorithm(request, startdAds, priority)))
				{
					// no preemptable resource offer either ... 
					dprintf(D_ALWAYS,
						"\t\t\tRejected (no preemptible offers)\n");
					sock.encode();
					if (!sock.put(REJECTED) || !sock.end_of_message())
					{
						dprintf (D_ALWAYS, "\t\t\tCould not send rejection\n");
						sock.end_of_message ();
						return MM_ERROR;
					}
					result = MM_NO_MATCH;
					continue;
				}
			}

			// 2e(ii).  perform the matchmaking protocol
			result = matchmakingProtocol (request, offer, startdPvtAds, sock);

			// 2e(iii). if the matchmaking protocol failed, do not consider the
			//			startd again for this negotiation cycle.
			if (result == MM_BAD_MATCH)
				startdAds.Delete (offer);
		}

		// 2f.  if MM_NO_MATCH was found for the request, get another request
		if (result == MM_NO_MATCH) 
		{
			i--;		// haven't used any resources this cycle
			continue;
		}

		// 2g.  move the offer from the startdAds to the consumedStartds so
		// 		that it will not be considered again in this negotiation cycle

		// consumedStartds.Insert (offer);  --- ClassAd code doesn't work 
		startdAds.Delete (offer);
	}

	// 3.  check if we hit out resource limit
	if (i == scheddLimit)
	{
		dprintf (D_ALWAYS, 	
				"\t\tReached schedd resource limit: %d ... stopping\n", i);
	}

	// break off negotiations
	if (!sock.put (END_NEGOTIATE) || !sock.end_of_message())
		dprintf (D_ALWAYS, "\t\tCould not send END_NEGOTIATE/eom\n");

	// ... and continue negotiating with others
	return MM_RESUME;
}


ClassAd *Matchmaker::
matchmakingAlgorithm(ClassAd &request,ClassAdList &startdAds,float preemptPrio)
{
	ClassAd 	*candidate;
	float		candidateRank;
	ClassAd 	*bestSoFar = NULL;	// using best match
	float		bestRank = -(FLT_MAX);
	ExprTree	*requestRank;
	ExprTree	*offerRank;
	ExprTree 	*matchCriterion;
	char 		buffer[80];
	char		remoteUser[128];
	EvalResult	result;

	// rank condition depends on whether we are in preemption mode or not
	matchCriterion = (preemptPrio >= 0) ? rankCondPreempt : rankCondStd;

	// stash the rank expression of the request
	requestRank = request.Lookup (ATTR_RANK);

	// scan the offer ads
	startdAds.Open ();
	while (candidate = startdAds.Next ())
	{
		// are we in preemption mode? 
		if (preemptPrio >= 0)
		{
			// if there is a remote user who has a higher priority, dont preempt
			if (candidate->LookupString (ATTR_REMOTE_USER, remoteUser) &&
				accountant.GetPriority(remoteUser) >= preemptPrio)
					continue;

			// if the PreemptionHold expression evaluates to true, dont preempt
			if (PreemptionHold 											&& 
				PreemptionHold->EvalTree(candidate,&request,&result)	&&
				result.type == LX_BOOL && result.b == TRUE)
					continue;
		}

		// the candidate offer and request must match
		if (*candidate == request)
		{
			// if the offer has no Rank expr, the symmetric match is enough
 			// if the offer has a Rank expr, the match criterion must hold
			offerRank = candidate->Lookup(ATTR_RANK);
			if ((!offerRank) || (offerRank && 
				matchCriterion->EvalTree(candidate,&request, &result) 	&&
				(result.type == LX_BOOL) && (result.b == TRUE)))
			{
				// check the rank of the request
				if (!requestRank)
				{
					// requestor doesn't have/use ranks --- use first fit
					startdAds.Close ();
					return candidate;
				}
				else
				{
					// calculate the request's rank of the offer
					if (!request.EvalFloat (ATTR_RANK,candidate,candidateRank))
					{
						// could not evaluate; hold old best rank
						continue;
					}

					// if this rank is the best so far, choose it
					if (candidateRank > bestRank) bestSoFar = candidate;
				}
			}
		}
		else
		{
			// fprintf (stderr, "Failed match\n");
		}
	}
	startdAds.Close ();


	// this is the best match
	return bestSoFar;
}


int Matchmaker::
matchmakingProtocol (ClassAd &request, ClassAd *offer, 
						ClassAdList &startdPvtAds, Sock &sock)
{
	int  cluster, proc;
	char startdAddr[32];
	char startdName[64];
	char *capability;
	ReliSock startdSock;

	// these will succeed
	request.LookupInteger (ATTR_CLUSTER_ID, cluster);
	request.LookupInteger (ATTR_PROC_ID, proc);

	// these should too, but may not
	if (!offer->LookupString (ATTR_STARTD_IP_ADDR, startdAddr)		||
		!offer->LookupString (ATTR_NAME, startdName))
	{
		dprintf (D_ALWAYS, "\t\t\tCould not lookup %s and %s\n", 
					ATTR_NAME, ATTR_STARTD_IP_ADDR);
		return MM_BAD_MATCH;
	}

	// find the startd's capability from the private ad
	if (!(capability = getCapability (startdName, startdPvtAds)))
	{
		dprintf(D_ALWAYS,"\t\t\t%s has no capability\n", startdName);
		return MM_BAD_MATCH;
	}
	
	// ---- real matchmaking protocol begins ----
	// 1.  contact the startd 
	dprintf (D_FULLDEBUG, "\t\t\tConnecting to startd %s at %s\n", 
				startdName, startdAddr); 
	startdSock.timeout (NegotiatorTimeout);
	if (!startdSock.connect (startdAddr, 0))
	{
		dprintf(D_ALWAYS,"\t\t\tCould not connect to %s\n", startdAddr);
		return MM_BAD_MATCH;
	}

	// 2.  pass the startd MATCH_INFO and capability strings
	dprintf (D_FULLDEBUG, "\t\t\tSending MATCH_INFO/capability( \"%s\" )/eom to"
				" startd", capability);
	startdSock.encode();
	if (!startdSock.put (MATCH_INFO) || 
		!startdSock.put (capability) || 
		!startdSock.end_of_message())
	{
		dprintf (D_ALWAYS,"\t\t\tCould not send MATCH_INFO/capability( \"%s\" )"
			"/eom to startd %s\n", capability, startdName);
		return MM_BAD_MATCH;
	}

	// 3.  send the match and capability to the schedd
	dprintf(D_FULLDEBUG,"\t\t\tSending PERMISSION with capability to schedd\n");
	sock.encode();
	if (!sock.put(PERMISSION) 	|| 
		!sock.put(capability)	||
		!sock.end_of_message())
	{
		dprintf (D_ALWAYS, "\t\t\tCould not send "
			"PERMISSION/capability ( \"%s\" ) to schedd\n", capability);
		return MM_ERROR;
	}

	// done
	dprintf (D_ALWAYS, "\t\t\tSuccessfully matched with %s\n", startdName);
}


void Matchmaker::
calculateNormalizationFactor (ClassAdList &scheddAds, float &max, 
				float &normalFactor)
{
	ClassAd *ad;
	char	scheddName[64];
	float	prio;

	// find the maximum of the priority values (i.e., lowest priority)
	max = FLT_MIN;
	scheddAds.Open();
	while (ad = scheddAds.Next())
	{
		// this will succeed (comes from collector)
		ad->LookupString (ATTR_NAME, scheddName);
		prio = accountant.GetPriority (scheddName);
		if (prio > max) max = prio;
	}
	scheddAds.Close();

	// calculate the normalization factor, i.e., sum of the (max/scheddprio)
	normalFactor = 0.0;
	scheddAds.Open();
	while (ad = scheddAds.Next())
	{
		ad->LookupString (ATTR_NAME, scheddName);
		prio = accountant.GetPriority (scheddName);
		normalFactor = normalFactor + max/prio;
	}
	scheddAds.Close();

	// done
	return;
}


char *Matchmaker::
getCapability (char *startdName, ClassAdList &startdPvtAds)
{
	ClassAd *pvtAd;
	static char	capability[80];
	char	name[64];

	startdPvtAds.Open();
	while (pvtAd = startdPvtAds.Next())
	{
		if (pvtAd->LookupString (ATTR_NAME, name)	&&
			strcmp (name, startdName) == 0			&&
			pvtAd->LookupString (ATTR_CAPABILITY, capability))
		{
			startdPvtAds.Close ();
			return capability;
		}
	}
	startdPvtAds.Close();
	return NULL;
}
