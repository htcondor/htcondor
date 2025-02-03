/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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
#include <math.h>
#include <float.h>
#include "condor_state.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_attributes.h"
#include "my_username.h"
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_uid.h"
#include "daemon.h"
#include "basename.h"
#include "condor_distribution.h"
#include "subsystem_info.h"
#include "dc_collector.h"

// Globals


double priority = 0.00001;
const char *pool = NULL;
struct 	PrioEntry { 
	PrioEntry(const std::string &name, float prio) : name(name), prio(prio) {}
	std::string name; 
	float prio;
};
static  std::vector<PrioEntry> prioTable;
#ifndef WIN32
#endif
ExprTree *rankCondStd;// no preemption or machine rank-preemption 
							  // i.e., (Rank > CurrentRank)
ExprTree *rankCondPrioPreempt;// prio preemption (Rank >= CurrentRank)
ExprTree *PreemptionReq;	// only preempt if true
ExprTree *PreemptionRank; 	// rank preemption candidates

void usage(const char *name, int iExitCode=1);

bool
obtainAdsFromCollector (ClassAdList &startdAds, const char *constraint)
{
	CondorQuery startdQuery(STARTD_AD);
	QueryResult result;

	// Use CondorQuery object to fetch ads of startds according 
	// to the constraint.
	if ( constraint ) {
		if ( startdQuery.addANDConstraint(constraint) != Q_OK )
			return false;		
	} 

	if ( pool ) {
		result = startdQuery.fetchAds(startdAds,pool);
	}
	else {
		CollectorList * collectors = CollectorList::create();
		result = collectors->query (startdQuery, startdAds);
		delete collectors;
	}

	if (result != Q_OK) 
		return false;

	return true;
}

ClassAd *
giveBestMachine(ClassAd &request,ClassAdList &startdAds,
			double   /*preemptPrio*/)
{
		// the order of values in this enumeration is important!
		// it goes from least preffered to most preffered, i.e. we
		// prefer a match with NO_PREEMPTION best.
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
	classad::Value	result;
	bool			val;
	float			tmp;
	



	// scan the offer ads

	startdAds.Open ();
	while ((candidate = startdAds.Next ())) {

		// the candidate offer and request must match
		if( !IsAMatch( &request, candidate ) ) {
				// they don't match; continue
			//printf("DEBUG: MATCH FAILED\n\nCANDIDATE:\n");
			//fPrintAd(stdout, *candidate);
			//printf("\nDEBUG: REQUEST:\n");
			//fPrintAd(stdout, request);
			continue;
		}

		candidatePreemptState = NO_PREEMPTION;
		// if there is a remote user, consider preemption ....
		if (candidate->LookupString (ATTR_ACCOUNTING_GROUP, remoteUser, sizeof(remoteUser)) ||
			candidate->LookupString (ATTR_REMOTE_USER, remoteUser, sizeof(remoteUser))) 
		{
				// check if we are preempting for rank or priority
			if( EvalExprToBool( rankCondStd, candidate, &request, result ) &&
				result.IsBooleanValue( val ) && val ) {
					// offer strictly prefers this request to the one
					// currently being serviced; preempt for rank
				candidatePreemptState = RANK_PREEMPTION;
			} else {
					// RemoteUser on machine has *worse* priority than request
					// so we can preempt this machine *but* we need to check
					// on two things first
				candidatePreemptState = PRIO_PREEMPTION;
					// (1) we need to make sure that PreemptionReq's hold (i.e.,
					// if the PreemptionReq expression isn't true, dont preempt)
				if (PreemptionReq && 
					!(EvalExprToBool(PreemptionReq,candidate,&request,result) &&
					  result.IsBooleanValue(val) && val) ) {
					continue;
				}
					// (2) we need to make sure that the machine ranks the job
					// at least as well as the one it is currently running 
					// (i.e., rankCondPrioPreempt holds)
				if(!(EvalExprToBool(rankCondPrioPreempt,candidate,&request,result)&&
					 result.IsBooleanValue(val) && val ) ) {
						// machine doesn't like this job as much -- find another
					continue;
				}
			} 
		}


		// calculate the request's rank of the offer
		if(!EvalFloat(ATTR_RANK,&request,candidate,tmp)) {
			tmp = 0.0;
		}
		candidateRankValue = tmp;

		// the quality of a match is determined by a lexicographic sort on
		// the following values, but more is better for each component.  
		// The standard condor_negotiator works in this order of preference:
		//  1. job rank of offer 
		//	2. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
		//  3. preemption rank (if preempting)
		//
		// But the below code for condor_find_host places the desires of the
		// system ahead of the desires of the resource requestor.  Thus the code
		// below works in the following order of preference:
		//	1. preemption state (2=no preempt, 1=rank-preempt, 0=prio-preempt)
		//  2. preemption rank (if preempting)
		//  3. job rank of offer 

		newBestFound = false;
		candidatePreemptRankValue = -(FLT_MAX);
		if( candidatePreemptState != NO_PREEMPTION ) {
			// calculate the preemption rank
			double rval;
			if( PreemptionRank &&
				EvalExprToNumber(PreemptionRank,candidate,&request,result) &&
				result.IsNumber(rval)) {

				candidatePreemptRankValue = rval;
			} else if( PreemptionRank ) {
				dprintf(D_ALWAYS, "Failed to evaluate PREEMPTION_RANK "
					"expression to a float.\n");
			}
		}
/**** old negotiator-style code: ***
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
******************************/
		if( candidatePreemptState > bestPreemptState ) {	// first by preempt state
			newBestFound = true;
		} else if( candidatePreemptState==bestPreemptState &&  // then by preempt rank				
				bestPreemptState != NO_PREEMPTION ) {
				// check if the preemption rank is better than the best
			if( candidatePreemptRankValue > bestPreemptRankValue ) {
				newBestFound = true;
			}
		} 
		if( (candidatePreemptState==bestPreemptState &&
			( (bestPreemptState == NO_PREEMPTION) ||
			  ((bestPreemptState != NO_PREEMPTION) && (fabs(candidatePreemptRankValue - bestPreemptRankValue) < 0.001))
			)) 
			&& (candidateRankValue > bestRankValue) )	// finally by job rank
		{
			newBestFound = true;
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

void
make_request_ad(ClassAd & requestAd, const char *rank)
{
	SetMyTypeName (requestAd, JOB_ADTYPE);
	requestAd.Assign(ATTR_TARGET_TYPE, STARTD_OLD_ADTYPE); // For pre 23.0 

	get_mySubSystem()->setTempName( "SUBMIT" );
	config_fill_ad( &requestAd );
	get_mySubSystem()->resetTempName( );

	get_mySubSystem()->setTempName( "TOOL" );
	config_fill_ad( &requestAd );
	get_mySubSystem()->resetTempName( );

	requestAd.Assign(ATTR_INTERACTIVE, true);
	requestAd.Assign(ATTR_SUBMITTOR_PRIO, priority);


	// Always set Requirements to True - we can do this because we only
	// fetch the startd ads which match the user's constraint in the first place.
	requestAd.Assign(ATTR_REQUIREMENTS, true);

	if( !rank ) {
		requestAd.Assign(ATTR_RANK, 0);
	} else {
		/* Tricksy use of AssignExpr here since rank will be a rhs expr */
		requestAd.AssignExpr(ATTR_RANK, rank);
	}

	requestAd.Assign(ATTR_Q_DATE, time(nullptr));

	char *owner = my_username();
	if( !owner ) {
		owner = strdup("unknown");
	}
	requestAd.Assign(ATTR_OWNER, owner);
	free(owner);

#ifdef WIN32
	// put the NT domain into the ad as well
	char *ntdomain = strdup(get_condor_username());
	if (ntdomain) {
		char *slash = strchr(ntdomain,'/');
		if ( slash ) {
			*slash = '\0';
			if ( strlen(ntdomain) > 0 ) {
				if ( strlen(ntdomain) > 80 ) {
					fprintf(stderr,"NT DOMAIN OVERFLOW (%s)\n",ntdomain);
					exit(1);
				}
				requestAd.Assign(ATTR_NT_DOMAIN, ntdomain);
			}
		}
		free(ntdomain);
	}
#endif
		
	requestAd.Assign(ATTR_JOB_REMOTE_USER_CPU, 0.0);
	requestAd.Assign(ATTR_JOB_REMOTE_SYS_CPU, 0.0);
	requestAd.Assign(ATTR_JOB_EXIT_STATUS, 0);
	requestAd.Assign(ATTR_NUM_CKPTS, 0);
	requestAd.Assign(ATTR_NUM_RESTARTS, 0);
	requestAd.Assign(ATTR_JOB_COMMITTED_TIME, 0);
	requestAd.Assign(ATTR_IMAGE_SIZE, 0);
	requestAd.Assign(ATTR_EXECUTABLE_SIZE, 0);
	requestAd.Assign(ATTR_DISK_USAGE, 0);

	// what else?
}


static void
fetchSubmittorPrios()
{
	ClassAd	al;
	char  	attrName[32], attrPrio[32];
  	char  	name[128];
  	double 	sub_priority;
	int		i = 1;

		// Minor hack, if we're talking to a remote pool, assume the
		// negotiator is on the same host as the collector.
	Daemon	negotiator( DT_NEGOTIATOR, NULL, pool );

	Sock* sock;

	if (!(sock = negotiator.startCommand( GET_PRIORITY, Stream::reli_sock, 0))) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		exit( 1 );
	}

	// connect to negotiator

	sock->end_of_message();
	sock->decode();
	if( !getClassAdNoTypes(sock, al) || !sock->end_of_message() ) {
		fprintf( stderr, 
				 "Error:  Could not get priorities from negotiator (%s)\n",
				 negotiator.fullHostname() );
		exit( 1 );
	}
	sock->close();
	delete sock;

	i = 1;
	while( i ) {
		snprintf( attrName, sizeof(attrName), "Name%d", i );
		snprintf( attrPrio, sizeof(attrPrio), "Priority%d", i );

    	if( !al.LookupString( attrName, name, sizeof(name) ) || 
			!al.LookupFloat( attrPrio, sub_priority ) )
            break;

		prioTable.emplace_back(name, sub_priority);
		i++;
	}

	if( i == 1 ) {
		printf( "Warning:  Found no submitters\n" );
	}
}

static int
findSubmittor( char *name ) 
{
	std::string sub(name);
	int			last = prioTable.size();
	int			i;
	
	for( i = 0 ; i < last ; i++ ) {
		if( prioTable[i].name == sub ) return i;
	}

	prioTable.emplace_back(sub, 0.5);

	return i;
}

void
usage(const char *name, int iExitCode)
{
	printf("\nUsage: %s [-m] -[n number] [-c c_expr] [-r r_expr] [-p pool] \n", name);
	printf(" -m: Return entire machine, not slots\n");
	printf(" -n num: Return num machines, where num is an integer "
			"greater than zero\n");
	printf(" -c c_expr: Use c_expr as the constraint expression\n");
	printf(" -r r_expr: Use r_expr as the rank expression\n");
	printf(" -p pool: Contact the Condor pool \"pool\"\n");
	printf(" -h: this screen\n\n");
	exit(iExitCode);
}


int
main(int argc, char *argv[])
{
	ClassAdList startdAds;
	char	**ptr;
	bool WantMachineNames = false;
	int NumMachinesWanted = 1;
	const char *constraint = NULL;
	const char *rank = NULL;
	ClassAd requestAd;
	ClassAd *offer;
	char *tmp;
	int i;
	int iExitUsageCode=1;
	char buffer[1024];
	std::map<std::string, int> slot_counts;

	set_priv_initialize(); // allow uid switching if root
	config();

	// parse command line args
	for( ptr=argv+1,argc--; argc > 0; argc--,ptr++ ) {
		if( ptr[0][0] == '-' ) {
			switch( ptr[0][1] ) {
			case 'm':	// want real machines, not slots
				WantMachineNames = true;
				break;
			case 'n':	// want specific number of machines
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -n requires another argument\n", 
							 condor_basename(argv[0]) );
					exit(1);
				}					
				NumMachinesWanted = atoi(*ptr);
				if ( NumMachinesWanted < 1 ) {
					fprintf( stderr, "%s: -n requires another argument "
							 "which is an integer greater than 0\n",
							 condor_basename(argv[0]) );
					exit(1);
				}
				break;
			case 'p':	// pool							
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -p requires another argument\n", 
							 condor_basename(argv[0]) );
					exit(1);
				}
				pool = *ptr;
				break;
			case 'c':	// constraint						
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -c requires another argument\n", 
							 condor_basename(argv[0]) );
					exit(1);
				}
				constraint = *ptr;
				break;
			case 'r':	// rank
				if( !(--argc) || !(*(++ptr)) ) {
					fprintf( stderr, "%s: -r requires another argument\n", 
							 condor_basename(argv[0]) );
					exit(1);
				}
				rank = *ptr;
				break;
			case 'h': 
			      iExitUsageCode = 0;
				  // Fall through to...
			      //@fallthrough@
			default:
				usage(condor_basename(argv[0]), iExitUsageCode);
			}		
		}
	}

	// initialize some global expressions
	snprintf (buffer, sizeof(buffer), "MY.%s > MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buffer, rankCondStd);
	snprintf (buffer, sizeof(buffer), "MY.%s >= MY.%s", ATTR_RANK, ATTR_CURRENT_RANK);
	ParseClassAdRvalExpr (buffer, rankCondPrioPreempt);

	// get PreemptionReq expression from config file
	PreemptionReq = NULL;
	tmp = param("PREEMPTION_REQUIREMENTS");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, PreemptionReq) ) {
			fprintf(stderr, 
				"\nERROR: Failed to parse PREEMPTION_REQUIREMENTS.\n");
			exit(1);
		}
	}

	// get PreemptionRank expression from config file
	PreemptionRank = NULL;
	tmp = param("PREEMPTION_RANK");
	if( tmp ) {
		if( ParseClassAdRvalExpr(tmp, PreemptionRank) ) {
			fprintf(stderr, 
				"\nERROR: Failed to parse PREEMPTION_RANK.\n");
			exit(1);
		}
	}

	// make the request ad
	make_request_ad(requestAd,rank);

	// grab the startd ads from the collector
	if ( !obtainAdsFromCollector(startdAds, constraint) ) {
		fprintf(stderr,
			"\nERROR:  failed to fetch startd ads ... aborting\n");
		exit(1);
	}

	// fetch submittor prios
	fetchSubmittorPrios();

	// populate startd ads with remote user prios
	ClassAd *ad;
	char remoteUser[1024];
	int index;
	startdAds.Open();
	while( ( ad = startdAds.Next() ) ) {
		if( ad->LookupString( ATTR_ACCOUNTING_GROUP , remoteUser, sizeof(remoteUser) ) ||
			ad->LookupString( ATTR_REMOTE_USER , remoteUser, sizeof(remoteUser) )) 
		{
			if( ( index = findSubmittor( remoteUser ) ) != -1 ) {
				ad->Assign( ATTR_REMOTE_USER_PRIO,
							prioTable[index].prio );
			}
		}
	}
	startdAds.Close();
	

	// find best machines and display them
	std::string remoteHost;
	for ( i = 0; i < NumMachinesWanted; i++ ) {
		
		offer = giveBestMachine(requestAd,startdAds,priority);
		
		// check if we found a machine
		if (!offer) {
			fprintf(stderr,
				"\nERROR: %d machines not available\n",NumMachinesWanted);
			exit(i+1);
		}

		// If we want the entire machine, and not just a slot...
		if(WantMachineNames) {
			if (offer->LookupString (ATTR_MACHINE, remoteHost) ) {
				int slot_count;

				// How many slots are on that machine?
				if (!offer->LookupInteger(ATTR_TOTAL_SLOTS, slot_count)) {
					slot_count = 1;
				}

				// If we don't have enough slots to complete the set,
				// stick it in the map, remove it from the list
				// of startd ads, and keep looking.
				// FIXME(?) This would probably blow up with bogus ads 
				// (ie duplicate ads, but I dunno if those can happen)
				if(++slot_counts[remoteHost] < slot_count) {
					//printf("DEBUG: Adding %s with %d\n", remoteHost.c_str(),
					//		slot_counts[remoteHost]);
					startdAds.Delete(offer);
					i--;
					continue;
				}
			}
		} //end if(WantMachineNames) 

		// here we found a machine; spit out the name to stdout
		remoteHost = "";
		if(WantMachineNames)
			offer->LookupString(ATTR_MACHINE, remoteHost);
		else
			offer->LookupString(ATTR_NAME, remoteHost);

		if (!remoteHost.empty()) {
			printf("%s\n", remoteHost.c_str());
		}

		// remote this startd ad from our list 
		startdAds.Delete(offer);
	}

	return 0;
}

