#include "condor_common.h"
#include "condor_state.h"
#include "condor_attributes.h"
#include "MyString.h"
#include "stdio.h"

#ifdef __GNUG__
#include "status_types.h"
class ClassTotal;
#pragma implementation "HashTable.h"
#include "HashTable.h"
typedef HashTable<MyString,ClassTotal*> _statusHashTable;
#endif

#include "totals.h"

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
displayTotals (int keyLength)
{
	ClassTotal *ct;
	MyString	key;

	// display totals only for meaningful modes
	switch (ppo)
	{
    	case PP_STARTD_NORMAL:
    	case PP_STARTD_SERVER:
    	case PP_STARTD_RUN:
    	case PP_SCHEDD_NORMAL:
    	case PP_SCHEDD_SUBMITTORS:   
    	case PP_CKPT_SRVR_NORMAL:
			break;

		default:
			return;
	}

		
	// display the lead of the header
	printf ("%*.*s", keyLength, keyLength, "");
	topLevelTotal->displayHeader();
	printf ("\n");

	allTotals.startIterations();
	while (allTotals.iterate(key, ct))
	{
		printf ("%*.*s", keyLength, keyLength, key.Value());
		ct->displayInfo();
	}
	printf ("\n%*.*s", keyLength, keyLength, "Total");
	topLevelTotal->displayInfo(1);

	if (malformed > 0)
	{
		printf ("\n%*.*s(Omitted %d malformed ads in computed totals)\n\n",
			keyLength, keyLength, "", malformed);
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
displayHeader()
{
	printf ("%9.9s %5.5s %7.7s %9.9s %7.7s %10.10s\n", "Machines", 
					"Owner", "Claimed", "Unclaimed", "Matched", "Preempting");
}


void StartdNormalTotal::
displayInfo (int)
{
	printf ("%9d %5d %7d %9d %7d %10d\n", machines, owner, claimed,
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
	State s;

	if (!ad->LookupString (ATTR_STATE, state)		||
		!ad->LookupInteger(ATTR_MEMORY,attrMem)		||
		!ad->LookupInteger(ATTR_DISK,  attrDisk)	||
		!ad->LookupInteger(ATTR_MIPS,  attrMips)	||
		!ad->LookupInteger(ATTR_KFLOPS,attrKflops))
			return 0;


	s = string_to_state(state);
	if (s == claimed_state || s == unclaimed_state)
			avail++;

	machines++;
	memory 	+= attrMem;
	disk   	+= attrDisk;
	condor_mips	+= attrMips;
	kflops	+= attrKflops;

	return 1;
}


void StartdServerTotal::
displayHeader()
{
	printf ("%9.9s %5.5s %7.7s %11.11s %11.11s %11.11s\n", "Machines", 
					"Avail", "Memory", "Disk", "MIPS", "KFLOPS");
}


void StartdServerTotal::
displayInfo (int)
{
	printf ("%9d %5d %7d %11d %11d %11d\n", machines, avail, memory,
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

	if (!ad->LookupInteger(ATTR_MIPS, attrMips)		||
		!ad->LookupInteger(ATTR_KFLOPS, attrKflops)	||
		!ad->LookupFloat  (ATTR_LOAD_AVG, attrLoadAvg))
			return 0;

	condor_mips += attrMips;
	kflops += kflops;
	loadavg += attrLoadAvg;	
	machines ++;

	return 1;
}


void StartdRunTotal::
displayHeader()
{
	printf ("%9.9s  %11.11s  %11.11s  %11.11s\n", "Machines", "MIPS", "KFLOPS",
				"AvgLoadAvg");
}


void StartdRunTotal::
displayInfo (int)
{
	printf ("%9d  %11d  %11d   %-.3f\n", machines, condor_mips, kflops, 
				(machines > 0) ? float(loadavg/machines) : 0);
}


ScheddNormalTotal::
ScheddNormalTotal()
{
	runningJobs = 0;
	idleJobs = 0;
}


int ScheddNormalTotal::
update (ClassAd *ad)
{
	int attrRunning, attrIdle;

	if (!ad->LookupInteger(ATTR_TOTAL_RUNNING_JOBS, attrRunning)	||
		!ad->LookupInteger(ATTR_TOTAL_IDLE_JOBS, attrIdle))
			return 0;

	runningJobs += attrRunning;	
	idleJobs += attrIdle;

	return 1;
}

	
void ScheddNormalTotal::
displayHeader()
{
	printf ("%18s   %18s\n", "TotalRunningJobs", "TotalIdleJobs");
}


void ScheddNormalTotal::
displayInfo (int tl)
{
	if (tl) printf ("%18d   %18d\n", runningJobs, idleJobs);
}



ScheddSubmittorTotal::
ScheddSubmittorTotal()
{
	runningJobs = 0;
	idleJobs = 0;
}


int ScheddSubmittorTotal::
update (ClassAd *ad)
{
	int attrRunning, attrIdle;

	if (!ad->LookupInteger(ATTR_RUNNING_JOBS, attrRunning)	||
		!ad->LookupInteger(ATTR_IDLE_JOBS, attrIdle))
			return 0;

	runningJobs += attrRunning;	
	idleJobs += attrIdle;

	return 1;
}

	
void ScheddSubmittorTotal::
displayHeader()
{
	printf ("%18s   %18s\n", "RunningJobs", "IdleJobs");
}


void ScheddSubmittorTotal::
displayInfo (int)
{
	printf ("%18d   %18d\n", runningJobs, idleJobs);
}


CkptSrvrNormalTotal::
CkptSrvrNormalTotal()
{
	numServers = 0;
}

int CkptSrvrNormalTotal::
update (ClassAd *)
{
	numServers++;
	return 1;
}

void CkptSrvrNormalTotal::
displayHeader()
{
	printf ("%-10.10s\n", "NumCkptServers");
}

void CkptSrvrNormalTotal::
displayInfo (int tl)
{
	if (tl) printf ("%10d\n", numServers);
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
	char p1[32], p2[32], buf[64];

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

