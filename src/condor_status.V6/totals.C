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
#include "condor_state.h"
#include "condor_attributes.h"
#include "MyString.h"
#include "stdio.h"
#include "HashTable.h"
#include "totals.h"

#ifndef WIN32
template class  HashTable<MyString,ClassTotal*>;
#endif

template class  HashBucket<MyString,ClassTotal*>;

int hashFunc (const MyString &key, int size)
{
	int j = 0;
	for (int i = key.Length()-1; i; i--) j+= key[i];
	return (j % size);
}

TrackTotals::
TrackTotals (ppOption m) : allTotals(16, hashFunc)
{
	ppo = m;
	malformed = 0;
	topLevelTotal = ClassTotal::makeTotalObject(ppo);
}

TrackTotals::
~TrackTotals ()
{
	ClassTotal *ct;

	allTotals.startIterations();
	while (allTotals.iterate(ct))
		delete ct;
	delete topLevelTotal;
}

int TrackTotals::
update (ClassAd *ad)
{
	ClassTotal *ct;
	MyString	key;
	int		   	rval;

	if (!ClassTotal::makeKey(key, ad, ppo)) 
	{
		malformed++;
		return 0;
	}

	if (allTotals.lookup (key, ct) < 0)
	{
		ct = ClassTotal::makeTotalObject (ppo);
		if (!ct) return 0;
		if (allTotals.insert (key, ct) < 0)
		{
			delete ct;
			return 0;
		}
	}

	rval = ct->update(ad);
	topLevelTotal->update(ad);

	if (rval == 0) malformed++;

	return rval;
}


void TrackTotals::
displayTotals (FILE *file, int keyLength)
{
	ClassTotal *ct;
	MyString	key;
	int k;

	// display totals only for meaningful modes
	switch (ppo)
	{
    	case PP_STARTD_NORMAL:
    	case PP_STARTD_SERVER:
    	case PP_STARTD_RUN:
		case PP_STARTD_STATE:
    	case PP_SCHEDD_NORMAL:
    	case PP_SCHEDD_SUBMITTORS:   
    	case PP_CKPT_SRVR_NORMAL:
			break;

		default:
			return;
	}

		
	// display the lead of the header
	fprintf (file, "%*.*s", keyLength, keyLength, "");
	topLevelTotal->displayHeader(file);
	fprintf (file, "\n");

	// sort the keys (insertion sort) so we display totals in sorted order
	const char **keys = new const char* [allTotals.getNumElements()];
	allTotals.startIterations();
	for (k = 0; k < allTotals.getNumElements(); k++) // for each key
	{
		allTotals.iterate(key, ct);
		// find the position where we want to insert the key
		int pos;
		for (pos = 0; pos < k && strcmp(keys[pos], key.Value()) < 0; pos++);
		if (pos < k) {
			// if we are not inserting at the end of the array, then
			// we must shift the elements to the right to make room;
			// we use memmove() because it handles overlapping buffers
			// correctly (and efficiently)
			memmove(keys+pos+1, keys+pos, (k-pos)*sizeof(char *));
		}
		// insert the key in the right position in the list
		keys[pos] = strdup(key.Value());
	}
	// now that our keys are sorted, display the totals in sort order
	for (k = 0; k < allTotals.getNumElements(); k++)
	{
		fprintf (file, "%*.*s", keyLength, keyLength, keys[k]);
		allTotals.lookup(MyString(keys[k]), ct);
		free((void *)keys[k]);
		ct->displayInfo(file);
	}
	delete [] keys;
	fprintf (file, "\n%*.*s", keyLength, keyLength, "Total");
	topLevelTotal->displayInfo(file,1);

	if (malformed > 0)
	{
		fprintf(file, "\n%*.*s(Omitted %d malformed ads in computed attribute "
					"totals)\n\n", keyLength, keyLength, "", malformed);
	}
}



StartdNormalTotal::
StartdNormalTotal()
{
	ppo = PP_STARTD_NORMAL;
	machines = 0;
	owner = 0;
	unclaimed = 0;
	claimed = 0;
	matched = 0;
	preempting = 0;
}


int StartdNormalTotal::
update (ClassAd *ad)
{
	char state[32];

	if (!ad->LookupString (ATTR_STATE, state)) return 0;
	switch (string_to_state (state))
	{
		case owner_state: 		owner++; 		break;
		case unclaimed_state: 	unclaimed++; 	break;
		case claimed_state:		claimed++;		break;
		case matched_state:		matched++;		break;
		case preempting_state:	preempting++;	break;
		default: return 0;
	}
	machines++;
	return 1;
}


void StartdNormalTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%9.9s %5.5s %7.7s %9.9s %7.7s %10.10s\n", "Machines", 
					"Owner", "Claimed", "Unclaimed", "Matched", "Preempting");
}


void StartdNormalTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%9d %5d %7d %9d %7d %10d\n", machines, owner, claimed,
					unclaimed, matched, preempting);
}


StartdServerTotal::
StartdServerTotal()
{
	ppo = PP_STARTD_SERVER;
	machines = 0;
	avail = 0;
	memory = 0;
	disk = 0;
	condor_mips = 0;
	kflops = 0;
}


int StartdServerTotal::
update (ClassAd *ad)
{
	char state[32];
	int	 attrMem, attrDisk, attrMips, attrKflops;
	bool badAd = false;
	State s;

	// if ATTR_STATE is not found, abort this ad
	if (!ad->LookupString (ATTR_STATE, state)) return 0;

	// for the other attributes, assume zero if absent
	if (!ad->LookupInteger(ATTR_MEMORY,attrMem)) { badAd = true; attrMem  = 0;}
	if (!ad->LookupInteger(ATTR_DISK,  attrDisk)){ badAd = true; attrDisk = 0;}
	if (!ad->LookupInteger(ATTR_MIPS,  attrMips)){ badAd = true; attrMips = 0;}
	if (!ad->LookupInteger(ATTR_KFLOPS,attrKflops)){badAd= true;attrKflops = 0;}

	s = string_to_state(state);
	if (s == claimed_state || s == unclaimed_state)
		avail++;

	machines++;
	memory 	+= attrMem;
	disk   	+= attrDisk;
	condor_mips	+= attrMips;
	kflops	+= attrKflops;

	// if some attribute was missing, report the ad as malformed
	if (badAd) return 0;

	return 1;
}


void StartdServerTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%9.9s %5.5s %7.7s %11.11s %11.11s %11.11s\n", "Machines", 
					"Avail", "Memory", "Disk", "MIPS", "KFLOPS");
}


void StartdServerTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%9d %5d %7d %11d %11d %11d\n", machines, avail, memory,
					disk, condor_mips, kflops);
}


StartdRunTotal::
StartdRunTotal()
{
	machines = 0;
	condor_mips = 0;
	kflops = 0;
	loadavg = 0;
}


int StartdRunTotal::
update (ClassAd *ad)
{
	int attrMips, attrKflops;
	float attrLoadAvg;
	bool badAd = false;

	if (!ad->LookupInteger(ATTR_MIPS, attrMips)) { badAd = true; attrMips = 0;}
	if (!ad->LookupInteger(ATTR_KFLOPS, attrKflops)){badAd=true; attrKflops=0;}
	if (!ad->LookupFloat(ATTR_LOAD_AVG,attrLoadAvg)){badAd=true;attrLoadAvg=0;}

	condor_mips += attrMips;
	kflops += attrKflops;
	loadavg += attrLoadAvg;	
	machines ++;

	// if some attribute was missing, report the ad as malformed
	if (badAd) return false;

	return 1;
}


void StartdRunTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%9.9s  %11.11s  %11.11s  %11.11s\n", "Machines", "MIPS", 
				"KFLOPS", "AvgLoadAvg");
}


void StartdRunTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%9d  %11d  %11d   %-.3f\n", machines, condor_mips, kflops, 
				(machines > 0) ? float(loadavg/machines) : 0);
}


StartdStateTotal::
StartdStateTotal()
{
	machines = 0;
	owner = 0;
	unclaimed = 0;
	claimed = 0;
	preempt = 0;
	matched = 0;
}

int StartdStateTotal::
update( ClassAd *ad )
{
	char	stateStr[32];
	State	state;

	machines ++;

	if( !ad->LookupString( ATTR_STATE , stateStr ) ) return false;
	state = string_to_state( stateStr );
	switch( state ) {
		case owner_state	:	owner++;		break;
		case unclaimed_state:	unclaimed++;	break;
		case claimed_state	:	claimed++;		break;
		case preempting_state:	preempt++;		break;
		case matched_state	:	matched++;		break;
		default				:	return false;
	}

	return 1;
}
		

void StartdStateTotal::
displayHeader(FILE *file)
{
	fprintf( file, "%10.10s %5.5s %9.9s %7.7s %10.10s %7.7s\n", "Machines", 
				"Owner", "Unclaimed", "Claimed", "Preempting", "Matched" );
}


void StartdStateTotal::
displayInfo( FILE *file, int )
{
	fprintf(file,"%10d %5d %9d %7d %10d %7d\n",machines,owner,unclaimed,claimed,
			preempt,matched);
}


ScheddNormalTotal::
ScheddNormalTotal()
{
	runningJobs = 0;
	idleJobs = 0;
	heldJobs = 0;
}


int ScheddNormalTotal::
update (ClassAd *ad)
{
	int attrRunning, attrIdle, attrHeld;;
	bool badAd = false;

	if (ad->LookupInteger(ATTR_TOTAL_RUNNING_JOBS, attrRunning)	) {
		runningJobs += attrRunning;
	} else {
		badAd = true;
	}
	if( ad->LookupInteger(ATTR_TOTAL_IDLE_JOBS, attrIdle) ) {
		idleJobs += attrIdle;
	} else {
		badAd = true;
	}
	if( ad->LookupInteger(ATTR_TOTAL_HELD_JOBS, attrHeld) ) {
		heldJobs += attrHeld;
	} else {
		badAd = true;
	}

	return !badAd;
}

	
void ScheddNormalTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%18s %18s %18s\n", "TotalRunningJobs", "TotalIdleJobs",
		"TotalHeldJobs");
}


void ScheddNormalTotal::
displayInfo (FILE *file, int tl)
{
	if (tl) fprintf(file,"%18d %18d %18d\n", runningJobs, idleJobs, heldJobs);
}



ScheddSubmittorTotal::
ScheddSubmittorTotal()
{
	runningJobs = 0;
	idleJobs = 0;
	heldJobs = 0;
}


int ScheddSubmittorTotal::
update (ClassAd *ad)
{
	int attrRunning=0, attrIdle=0, attrHeld=0;
	bool badAd = false;

	if( ad->LookupInteger(ATTR_RUNNING_JOBS, attrRunning) ) {
		runningJobs += attrRunning;	
	} else {
		badAd = true;
	}
	if( ad->LookupInteger(ATTR_IDLE_JOBS, attrIdle) ) {
		idleJobs += attrIdle;
	} else {
		badAd = true;
	}
	if( ad->LookupInteger(ATTR_HELD_JOBS, attrHeld) ) {
		heldJobs += attrHeld;
	} else {
		badAd = true;
	}

	return !badAd;
}

	
void ScheddSubmittorTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%18s %18s %18s\n", "RunningJobs", "IdleJobs", "HeldJobs");
}


void ScheddSubmittorTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%18d %18d %18d\n", runningJobs, idleJobs, heldJobs);
}


CkptSrvrNormalTotal::
CkptSrvrNormalTotal()
{
	numServers = 0;
	disk = 0;
}

int CkptSrvrNormalTotal::
update (ClassAd *ad)
{
	int attrDisk = 0;

	numServers++;

	if (!ad->LookupInteger(ATTR_DISK, attrDisk))
		return 0;

	disk += attrDisk;

	return 1;
}

void CkptSrvrNormalTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%8.8s %-9.9s", "Servers", "AvailDisk");
}

void CkptSrvrNormalTotal::
displayInfo (FILE *file, int tl)
{
	if (tl) fprintf (file, "%8d %9d\n", numServers, disk);
}

ClassTotal::
ClassTotal()
{
	ppo = PP_NOTSET;
}


ClassTotal::
~ClassTotal()
{
}


ClassTotal *ClassTotal::
makeTotalObject (ppOption ppo)
{
	ClassTotal *ct;

	switch (ppo)
	{
		case PP_STARTD_NORMAL: 		ct = new StartdNormalTotal; break;
		case PP_STARTD_SERVER:		ct = new StartdServerTotal;	break;
		case PP_STARTD_RUN:			ct = new StartdRunTotal;	break;
		case PP_STARTD_STATE:		ct = new StartdStateTotal;	break;
		case PP_SCHEDD_NORMAL:		ct = new ScheddNormalTotal; break;
		case PP_SCHEDD_SUBMITTORS:	ct = new ScheddSubmittorTotal; break;
		case PP_CKPT_SRVR_NORMAL:	ct = new CkptSrvrNormalTotal; break;

		default:
			return NULL;
	}

	return ct;
}


int ClassTotal::
makeKey (MyString &key, ClassAd *ad, ppOption ppo)
{
	char p1[256], p2[256], buf[512];

	switch (ppo)
	{
		case PP_STARTD_NORMAL:
		case PP_STARTD_RUN:
		case PP_STARTD_SERVER:
			if (!ad->LookupString(ATTR_ARCH, p1) || 
				!ad->LookupString(ATTR_OPSYS, p2))
					return 0;
			sprintf(buf, "%s/%s", p1, p2);
			key = buf;
			return 1;

		case PP_STARTD_STATE:
			if( !ad->LookupString( ATTR_ACTIVITY , p1 ) )
				return 0;
			sprintf( buf, "%s", p1 );
			key = buf;
			return 1;

		case PP_SCHEDD_SUBMITTORS:
			if (!ad->LookupString(ATTR_NAME, p1)) return 0;
			key = p1;
			return 1;

		// all ads in the following categories hash to the same key for totals
		case PP_CKPT_SRVR_NORMAL:
		case PP_SCHEDD_NORMAL:
			key = " ";
			return 1;

		default:
			return 0;
	}
}

