/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_state.h"
#include "enum_utils.h"
#include "condor_attributes.h"
#include "stdio.h"
#include "totals.h"
#include "stl_string_utils.h"
#include "ad_printmask.h" // for format_platform_name

#ifndef WIN32
#endif


TrackTotals::
TrackTotals (ppOption m)
{
	ppo = m;
	malformed = 0;
	topLevelTotal = ClassTotal::makeTotalObject(ppo);
}

TrackTotals::
~TrackTotals ()
{
	for (auto& [key, ct] : allTotals) {
		delete ct;
	}
	delete topLevelTotal;
}

int TrackTotals::
update (ClassAd *ad, int options, const char * _key /*=""*/)
{
	ClassTotal *ct;
	std::string key(_key ? _key : "");
	int		   	rval;

	if ( key.empty() && ! ClassTotal::makeKey(key, ad, ppo))
	{
		malformed++;
		return 0;
	}

	auto itr = allTotals.find(key);
	if (itr == allTotals.end())
	{
		ct = ClassTotal::makeTotalObject (ppo);
		if (!ct) return 0;
		allTotals[key] = ct;
	} else {
		ct = itr->second;
	}

	rval = ct->update(ad, options);
	topLevelTotal->update(ad, options);

	if (rval == 0) malformed++;

	return rval;
}

bool TrackTotals::haveTotals()
{
	// display totals only for meaningful modes
	switch (ppo)
	{
    	case PP_SLOTS_NORMAL:
    	case PP_SLOTS_SERVER:
    	case PP_SLOTS_RUN:
		case PP_SLOTS_STATE:
    	case PP_SLOTS_COD:
    	case PP_SCHEDD_NORMAL:
    	case PP_SUBMITTER_NORMAL:
    	case PP_CKPT_SRVR_NORMAL:
		case PP_STARTDAEMON:
			break;

		default:
			return false;
	}
	return true;
}

void TrackTotals::addProjection(classad::References & attrs)
{
	if (topLevelTotal) { 
		topLevelTotal->addProjection(attrs);
	}
	return;
}


void TrackTotals::
displayTotals (FILE *file, int keyLength)
{
	bool auto_key_length = keyLength < 0;
	if (auto_key_length) { keyLength = 5; } // must be at least 5 for "Total"

	// display totals only for meaningful modes
	if ( ! haveTotals()) return;

	// Find the longest key length
	if (auto_key_length) {
		for (auto& [key, ct] : allTotals) {
			keyLength = MAX(keyLength, (int)key.length());
		}
	}

	// display the lead of the header
	fprintf (file, "%*.*s", keyLength, keyLength, "");
	topLevelTotal->displayHeader(file);
	fprintf (file, "\n");

	// display the totals
	// The map's comparison function gives us the entries sorted by key
	bool had_tot_keys = false;
	for (auto& [key, ct] : allTotals) {
		fprintf (file, "%*.*s", keyLength, keyLength, key.c_str());
		ct->displayInfo(file);
		had_tot_keys = true;
	}
	if (had_tot_keys) fprintf(file, "\n");
	fprintf (file, "%*.*s", keyLength, keyLength, "Total");
	topLevelTotal->displayInfo(file,1);

	if (malformed > 0)
	{
		fprintf(file, "\n%*.*s(Omitted %d malformed ads in computed attribute "
					"totals)\n\n", keyLength, keyLength, "", malformed);
	}
}

void StartDaemonTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_TOTAL_SLOTS, ATTR_NUM_DYNAMIC_SLOTS,
		ATTR_TOTAL_CPUS, ATTR_TOTAL_MEMORY, /*"TotalDisk",*/ "TotalGPUs", 
		"TotalInUseCpus", "TotalInUseMemory", "TotalInUseDisk", "TotalInUseGPUs",
		"TotalBackfillInUseCpus", "TotalBackfillInUseMemory", "TotalBackfillInUseDisk", "TotalBackfillInUseGPUs",
		ATTR_ARCH, ATTR_OPSYS, ATTR_OPSYS_AND_VER, ATTR_CONDOR_VERSION,	// for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartDaemonTotal::
update (ClassAd *ad, int /*options*/)
{
	bool bad = false;
	int total_slots = 0;
	long long total_mem = 0, inuse_mem=0, bk_mem=0;
	int total_cpus = 0, total_gpus = 0, inuse_cpus = 0, inuse_gpus = 0, bk_cpus = 0, bk_gpus = 0;
	if ( ! ad->LookupInteger(ATTR_TOTAL_SLOTS, total_slots) && ! ad->LookupInteger(ATTR_NUM_DYNAMIC_SLOTS, total_slots)) {
		bad = true; total_slots = 0;
	}

	if ( ! ad->LookupInteger(ATTR_TOTAL_CPUS, total_cpus)) { bad = true; total_cpus = 0; }
	if ( ! ad->LookupInteger("TotalGPUs", total_gpus)) { bad = true; total_gpus = 0; }
	if ( ! ad->LookupInteger(ATTR_TOTAL_MEMORY, total_mem)) { bad = true; total_mem = 0; }

	// TODO: handle p-slots that are simulating daemon ads ?
	if ( ! ad->LookupInteger("TotalInUseCpus", inuse_cpus)) { inuse_cpus = 0; }
	if ( ! ad->LookupInteger("TotalInUseGPUs", inuse_gpus)) { inuse_gpus = 0; }
	if ( ! ad->LookupInteger("TotalInUseMemory", inuse_mem)) { inuse_mem = 0; }
	if ( ! ad->LookupInteger("TotalBackfillInUseCpus", bk_cpus)) { bk_cpus = 0; }
	if ( ! ad->LookupInteger("TotalBackfillInUseGPUs", bk_gpus)) { bk_gpus = 0; }
	if ( ! ad->LookupInteger("TotalBackfillInUseMemory", bk_mem)) { bk_mem = 0; }

	machines += 1;
	slots += total_slots;
	cpus += total_cpus;
	gpus += total_gpus;
	busy_cpus += inuse_cpus;
	busy_gpus += inuse_gpus;
	bkfill_cpus += bk_cpus;
	bkfill_gpus += bk_gpus;

	if (total_mem != 0) {
		mem_usage += (double)inuse_mem / total_mem;
		bk_mem_usage += (double)bk_mem / total_mem;
	} else {
		bad = true;
	}

	// return 0 for malformed ads, 1 for success
	if (bad) return 0;
	return 1;
}

void StartDaemonTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
		"Machines", "Slots",
		"Cpus", "BusyCpus", "BkCpus",
		"GPUs", "BusyGPUs", "BkGPUs",
		"Memory%", "BkMem%");
}


void StartDaemonTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%8d %8d %8d %8d %8d %8d %8d %8d %7.2f%% %7.2f%%\n",
		machines, slots,
		cpus, busy_cpus, bkfill_cpus,
		gpus, busy_gpus, bkfill_gpus,
		100 * mem_usage/machines, 100 * bk_mem_usage/machines
		);
}

void StartdNormalTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_STATE,
		ATTR_ARCH, ATTR_OPSYS, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartdNormalTotal::
update (ClassAd *ad, int options)
{
	char state[32];

	bool partitionable_slot = false;
	bool dynamic_slot = false;
	bool backfill_slot = false;
	if (options) {
		ad->LookupBool(ATTR_SLOT_PARTITIONABLE, partitionable_slot);
		if ( ! partitionable_slot) { ad->LookupBool(ATTR_SLOT_DYNAMIC, dynamic_slot); }
		if (options & TOTALS_OPTION_BACKFILL_SLOTS) { ad->LookupBool(ATTR_SLOT_BACKFILL, backfill_slot); }
	}

	if ((options & TOTALS_OPTION_IGNORE_PARTITIONABLE) && partitionable_slot) return 1;
	if ((options & TOTALS_OPTION_IGNORE_DYNAMIC) && dynamic_slot) return 1;
	if ((options & TOTALS_OPTION_ROLLUP_PARTITIONABLE) && partitionable_slot) {
		classad::Value lval;
		const classad::ExprList* plist = NULL;
		if ( ! ad->EvaluateAttr("Child" ATTR_STATE, lval, classad::Value::ALL_VALUES) || ! lval.IsListValue(plist)) return 1; // ChildState can be validly empty
		for (auto it : *plist) {
			const char * cstr = nullptr;
			if (ExprTreeIsLiteralString(it, cstr) && cstr) {
				update(cstr, backfill_slot);
			}
		}
		return 1;
	} else {
		if (!ad->LookupString (ATTR_STATE, state, sizeof(state))) return 0;
		return update(state, backfill_slot);
	}
}

int StartdNormalTotal::
update (const char * state, bool backfill_slot)
{
	State st = string_to_state (state);
	if (backfill_slot) {
		if (st == unclaimed_state) {
			++backfill_idle;
			return 1; // don't count idle backfill slots in the machine total
		}
		if (st == claimed_state) st = backfill_state;
	}
	switch (st)
	{
		case owner_state: 		owner++; 		break;
		case unclaimed_state: 	unclaimed++; 	break;
		case claimed_state:		claimed++;		break;
		case matched_state:		matched++;		break;
		case preempting_state:	preempting++;	break;
		case backfill_state:	backfill++;		break;
		case drained_state:		drained++;	break;
		default: return 0;
	}
	machines++;
	return 1;
}


void StartdNormalTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%6.6s %5.5s %7.7s %9.9s %7.7s %10.10s %6.6s %8.8s %6.6s\n",
					"Total", "Owner", "Claimed", "Unclaimed", "Matched",
					"Preempting", "Drain", "Backfill", "BkIdle");
}


void StartdNormalTotal::
displayInfo (FILE *file, int)
{
	fprintf ( file, "%6d %5d %7d %9d %7d %10d %6d %8d %6d\n", machines, owner,
			  claimed, unclaimed, matched, preempting, drained, backfill, backfill_idle);

}


StartdServerTotal::
StartdServerTotal()
{
	ppo = PP_SLOTS_SERVER;
	machines = 0;
	avail = 0;
	memory = 0;
	disk = 0;
	condor_mips = 0;
	kflops = 0;
}

void StartdServerTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_STATE, ATTR_MEMORY, ATTR_DISK,	ATTR_MIPS, ATTR_KFLOPS,
		ATTR_ARCH, ATTR_OPSYS, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartdServerTotal::
update (ClassAd *ad, int options)
{
	char state[32];
	int	 attrMem, attrDisk, attrMips, attrKflops = 0;
	bool badAd = false;

	bool partitionable_slot = false;
	bool dynamic_slot = false;
	if (options) {
		ad->LookupBool(ATTR_SLOT_PARTITIONABLE, partitionable_slot);
		if ( ! partitionable_slot) { ad->LookupBool(ATTR_SLOT_DYNAMIC, dynamic_slot); }
	}

	// if ATTR_STATE is not found, abort this ad
	if (!ad->LookupString (ATTR_STATE, state, sizeof(state))) return 0;

	// for the other attributes, assume zero if absent
	if (!ad->LookupInteger(ATTR_MEMORY,attrMem)) { badAd = true; attrMem  = 0;}
	if (!ad->LookupInteger(ATTR_DISK,  attrDisk)){ badAd = true; attrDisk = 0;}
	if (!ad->LookupInteger(ATTR_MIPS,  attrMips)){ badAd = true; attrMips = 0;}
	if (!ad->LookupInteger(ATTR_KFLOPS,attrKflops)){badAd= true;attrKflops = 0;}

	State s = string_to_state(state);
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
	fprintf (file, "%9d %5d %11" PRIu64" %11" PRIu64" %11" PRIu64" %11" PRIu64"\n",
		 machines, avail, (PRIu64_t)memory,
			 (PRIu64_t)disk, (PRIu64_t)condor_mips, (PRIu64_t)kflops);
}


StartdRunTotal::
StartdRunTotal()
{
	machines = 0;
	condor_mips = 0;
	kflops = 0;
	loadavg = 0;
}

void StartdRunTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_MIPS, ATTR_KFLOPS, ATTR_LOAD_AVG,
		ATTR_ARCH, ATTR_OPSYS, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartdRunTotal::
update (ClassAd *ad, int options)
{
	int attrMips, attrKflops;
	double attrLoadAvg;
	bool badAd = false;

	bool partitionable_slot = false;
	bool dynamic_slot = false;

	if (options) {
		ad->LookupBool(ATTR_SLOT_PARTITIONABLE, partitionable_slot);
		if ( ! partitionable_slot) { ad->LookupBool(ATTR_SLOT_DYNAMIC, dynamic_slot); }
	}

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
	fprintf (file, "%9d  %11" PRIu64"  %11" PRIu64"   %-.3f\n",
		 machines, (PRIu64_t)condor_mips, (PRIu64_t)kflops, 
			 (machines > 0) ? float(loadavg/machines) : 0);
}

void StartdStateTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_STATE,
		ATTR_ACTIVITY, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartdStateTotal::
update (ClassAd *ad, int options)
{
	char state[32];

	bool partitionable_slot = false;
	bool dynamic_slot = false;
	bool backfill_slot = false;
	if (options) {
		ad->LookupBool(ATTR_SLOT_PARTITIONABLE, partitionable_slot);
		if ( ! partitionable_slot) { ad->LookupBool(ATTR_SLOT_DYNAMIC, dynamic_slot); }
		if (options & TOTALS_OPTION_BACKFILL_SLOTS) { ad->LookupBool(ATTR_SLOT_BACKFILL, backfill_slot); }
	}

	if ((options & TOTALS_OPTION_IGNORE_PARTITIONABLE) && partitionable_slot) return 1;
	if ((options & TOTALS_OPTION_IGNORE_DYNAMIC) && dynamic_slot) return 1;
	if ((options & TOTALS_OPTION_ROLLUP_PARTITIONABLE) && partitionable_slot) {
		classad::Value lval;
		const classad::ExprList* plist = NULL;
		if ( ! ad->EvaluateAttr("Child" ATTR_STATE, lval, classad::Value::ValueType::ALL_VALUES) || ! lval.IsListValue(plist)) return 1;
		for (auto it : *plist) {
			const char * cstr = nullptr;
			if (ExprTreeIsLiteralString(it, cstr) && cstr) {
				update(cstr, backfill_slot);
			}
		}
		return 1;
	} else {
		if (!ad->LookupString (ATTR_STATE, state, sizeof(state))) return 0;
		return update(state, backfill_slot);
	}
}


int StartdStateTotal::
update (const char * state, bool backfill_slot)
{
	State st = string_to_state (state);
	if (backfill_slot) {
		if (st == unclaimed_state) {
			++backfill_idle;
			return 1; // don't count idle backfill slots in the machine total
		}
		if (st == claimed_state) st = backfill_state;
	}
	switch (st) {
		case owner_state	:	owner++;		break;
		case unclaimed_state:	unclaimed++;	break;
		case claimed_state	:	claimed++;		break;
		case preempting_state:	preempt++;		break;
		case matched_state	:	matched++;		break;
		case backfill_state:	backfill++;		break;
		case drained_state:		drained++;	break;
		default				:	return false;
	}
	return 1;
}



void StartdStateTotal::
displayHeader(FILE *file)
{
	fprintf (file, "%6.6s %5.5s %9.9s %7.7s %10.10s %7.7s %6.6s %8.8s %6.6s\n",
					"Total", "Owner", "Unclaimed", "Claimed",
					"Preempting", "Matched", "Drain", "Backfill", "BkIdle");
}


void StartdStateTotal::
displayInfo( FILE *file, int )
{
	fprintf( file, "%6d %5d %9d %7d %10d %7d %6d %8d %6d\n", machines, owner,
			 unclaimed, claimed, preempt, matched, drained, backfill, backfill_idle);
}


StartdCODTotal::
StartdCODTotal()
{
	total = 0;
	idle = 0;
	running = 0;
	suspended = 0;
	vacating = 0;
	killing = 0;
}

void
StartdCODTotal::updateTotals( ClassAd* ad, const char* id ) 
{
	char* state_str = getCODStr( ad, id, ATTR_CLAIM_STATE, "unknown" );
	ClaimState	state = getClaimStateNum( state_str );
	free( state_str );
	switch( state ) {
	case CLAIM_IDLE:		idle++;			break;
	case CLAIM_RUNNING:		running++;		break;
	case CLAIM_SUSPENDED:	suspended++;	break;
	case CLAIM_VACATING:	vacating++;		break;
	case CLAIM_KILLING:		killing++;		break;
	default:	break;
	}
	total++;
}

void StartdCODTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_CLAIM_STATE, ATTR_COD_CLAIMS, ATTR_STATE,
		ATTR_ARCH, ATTR_OPSYS, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int StartdCODTotal::
update (ClassAd *ad, int /*options*/)
{
	std::string cod_claims;
	if (!ad->LookupString(ATTR_COD_CLAIMS, cod_claims)) {
		return 0;
	}
	for (auto& claim_id: StringTokenIterator(cod_claims)) {
		updateTotals( ad, claim_id.c_str() );
	}
	return 1;
}
		

void StartdCODTotal::
displayHeader(FILE *file)
{
	fprintf( file, "%8.8s %5.5s %8.8s %10.10s %9.9s %8.8s\n", "Total", 
				"Idle", "Running", "Suspended", "Vacating", "Killing" );
}


void StartdCODTotal::
displayInfo( FILE *file, int )
{
	fprintf( file, "%8d %5d %8d %10d %9d %8d\n", total, idle,
			 running, suspended, vacating, killing );
}

ScheddNormalTotal::
ScheddNormalTotal()
{
	runningJobs = 0;
	idleJobs = 0;
	heldJobs = 0;
}

void ScheddNormalTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_TOTAL_RUNNING_JOBS, ATTR_TOTAL_IDLE_JOBS, ATTR_TOTAL_HELD_JOBS,
	};
	for (auto key : keys) attrs.insert(key);
}

int ScheddNormalTotal::
update (ClassAd *ad, int /*options*/)
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

void ScheddSubmittorTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = {
		ATTR_RUNNING_JOBS, ATTR_IDLE_JOBS, ATTR_HELD_JOBS,
		ATTR_NAME, // for key
	};
	for (auto key : keys) attrs.insert(key);
}

int ScheddSubmittorTotal::
update (ClassAd *ad, int /*options*/)
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
	fprintf (file, "%11s %10s %10s\n", "RunningJobs", "IdleJobs", "HeldJobs");
}


void ScheddSubmittorTotal::
displayInfo (FILE *file, int)
{
	fprintf (file, "%11d %10d %10d\n", runningJobs, idleJobs, heldJobs);
}


CkptSrvrNormalTotal::
CkptSrvrNormalTotal()
{
	numServers = 0;
	disk = 0;
}

void CkptSrvrNormalTotal::addProjection(classad::References & attrs)
{
	static const char * const keys[] = { ATTR_DISK, };
	for (auto key : keys) attrs.insert(key);
}

int CkptSrvrNormalTotal::
update (ClassAd *ad, int /*options*/)
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
	fprintf (file, "%8.8s %-11.11s", "Servers", "AvailDisk");
}

void CkptSrvrNormalTotal::
displayInfo (FILE *file, int tl)
{
	if (tl) fprintf (file, "%8d %11" PRIu64"\n",
					 numServers, (PRIu64_t) disk);
}

ClassTotal *ClassTotal::
makeTotalObject (ppOption ppo)
{
	ClassTotal *ct;

	switch (ppo)
	{
		case PP_SLOTS_NORMAL: 		ct = new StartdNormalTotal; break;
		case PP_SLOTS_SERVER:		ct = new StartdServerTotal;	break;
		case PP_SLOTS_RUN:			ct = new StartdRunTotal;	break;
		case PP_SLOTS_STATE:		ct = new StartdStateTotal;	break;
		case PP_SLOTS_COD:			ct = new StartdCODTotal;	break;
		case PP_SCHEDD_NORMAL:		ct = new ScheddNormalTotal; break;
		case PP_SUBMITTER_NORMAL:	ct = new ScheddSubmittorTotal; break;
		case PP_CKPT_SRVR_NORMAL:	ct = new CkptSrvrNormalTotal; break;
		case PP_STARTDAEMON:		ct = new StartDaemonTotal; break;

		default:
			return NULL;
	}

	return ct;
}


/*static*/
bool ClassTotal::makeKey (std::string &key, ClassAd *ad, ppOption ppo)
{
	switch (ppo)
	{
		case PP_SLOTS_NORMAL:
		case PP_SLOTS_RUN:
		case PP_SLOTS_COD:
		case PP_SLOTS_SERVER:
		{
			char p1[128], p2[128];
			if (!ad->LookupString(ATTR_ARCH, p1, sizeof(p1)) ||
				!ad->LookupString(ATTR_OPSYS, p2, sizeof(p2)))
				return false;
			formatstr(key, "%s/%s", p1, p2);
			return true;
		}

		case PP_STARTDAEMON:
			return format_platform_name(key, ad);

		case PP_SLOTS_STATE:
			return ad->LookupString(ATTR_ACTIVITY, key);

		case PP_SUBMITTER_NORMAL:
			return ad->LookupString(ATTR_NAME, key);

		// all ads in the following categories hash to the same key for totals
		case PP_CKPT_SRVR_NORMAL:
		case PP_SCHEDD_NORMAL:
			key = " ";
			return true;

		default:
			return false;
	}
	return false;
}


int
getCODInt( ClassAd* ad, const char* id, const char* attr, int alt_val )
{
	int rval;
	char buf[128];
	snprintf( buf, sizeof(buf), "%s_%s", id, attr );
	if( ad->LookupInteger(buf, rval) ) {
		return rval;
	}
	return alt_val;
} 


char* 
getCODStr( ClassAd* ad, const char* id, const char* attr, 
		   const char* alt ) 
{
	char* tmp = NULL;
	char buf[128];
	snprintf( buf, sizeof(buf), "%s_%s", id, attr );
	ad->LookupString( buf, &tmp );
	if( tmp ) {
		return tmp;
	}
	return strdup( alt );
}

