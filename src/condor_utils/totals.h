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

#ifndef __TOTALS_H__
#define __TOTALS_H__

#include "condor_common.h"
#include "condor_classad.h"
#include "status_types.h"

// object keeps track of totals within a single class (same key)
// (i.e., "OpSys/Arch" for a startd, "Name" for schedd, etc.)
class ClassTotal
{
  	public:
		ClassTotal () = default;
		ClassTotal (ppOption opt) : ppo(opt) {}
		virtual ~ClassTotal () {};

		static ClassTotal *makeTotalObject(ppOption);
		static int makeKey( std::string &, ClassAd *, ppOption);

		virtual int update(ClassAd*, int options) 	= 0;
		virtual void displayHeader(FILE*)= 0;

		// param is zero for non-final totals
		virtual void displayInfo(FILE*, int=0)	= 0;

  	protected:
		ppOption ppo = PP_NOTSET;
};


#define TOTALS_OPTION_IGNORE_PARTITIONABLE 0x0001
#define TOTALS_OPTION_ROLLUP_PARTITIONABLE 0x0002
#define TOTALS_OPTION_IGNORE_DYNAMIC       0x0004
#define TOTALS_OPTION_BACKFILL_SLOTS       0x0008 // attribute claimed backfill slots as backfill state

// object manages totals for different classes within the same ppo
// (e.g., different "Arch/OpSys" in startd normal mode)
class TrackTotals
{
	public:
		TrackTotals (ppOption);
		~TrackTotals();

		int  update(ClassAd *, int options = 0, const char * key="");
		void displayTotals(FILE *, int keyLength);
		bool haveTotals();

	private:
		ppOption ppo;
		int malformed;
		std::map<std::string,ClassTotal*> allTotals;
		ClassTotal*	topLevelTotal;
};



// startd totals
class StartdNormalTotal : public ClassTotal
{
	public:
		StartdNormalTotal() : ClassTotal(PP_STARTD_NORMAL) {}
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE *);
		virtual void displayInfo(FILE *, int);

  	protected:
		int machines=0;
		int owner=0;
		int unclaimed=0;
		int claimed=0;
		int matched=0;
		int preempting=0;
		int drained=0;
		int backfill=0;
		int backfill_idle=0; // count of unclaimed backfill slots

		int update(const char * state, bool backfill_slot);
};


class StartdRunTotal : public ClassTotal
{
	public:
		StartdRunTotal();
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE *);
		virtual void displayInfo(FILE *,int);

	protected:
		int machines;
		uint64_t condor_mips;
		uint64_t kflops;
		float loadavg;
};


class StartdServerTotal : public ClassTotal
{
	public:
		StartdServerTotal();
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

  	protected:
		int machines;
		int avail;
		uint64_t memory;
		uint64_t disk;
		long condor_mips;
		uint64_t kflops;
};


class StartdStateTotal : public ClassTotal
{
	public:
		StartdStateTotal() : ClassTotal(PP_STARTD_STATE) {};
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

  	protected:
		int machines=0;
		int owner=0;
		int unclaimed=0;
		int claimed=0;
		int preempt=0;
		int matched=0;
		int drained=0;
		int backfill=0;
		int backfill_idle=0; // count of unclaimed backfill slots

		int update(const char * state, bool backfill_slot);
};



class StartdCODTotal : public ClassTotal
{
public:
	StartdCODTotal();
	virtual int update (ClassAd *, int options);
	virtual void displayHeader(FILE*);
	virtual void displayInfo(FILE*,int);
protected:
	void updateTotals( ClassAd*, const char* );

	int total;
	int idle;
	int running;
	int suspended;
	int vacating;
	int killing;
};


// schedd totals
class ScheddNormalTotal : public ClassTotal
{
	public:
		ScheddNormalTotal();
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

	protected:
		int runningJobs;
		int idleJobs;
		int heldJobs;
};


class ScheddSubmittorTotal : public ClassTotal
{
	public:
		ScheddSubmittorTotal();
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

	protected:
		int runningJobs;
		int idleJobs;
		int heldJobs;
};


// ckptserver totals
class CkptSrvrNormalTotal : public ClassTotal
{
	public:
		CkptSrvrNormalTotal();
		virtual int update (ClassAd *, int options);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

	protected:
		int numServers;
		uint64_t disk;
};

int getCODInt( ClassAd* ad, const char* id, const char* attr,
			   int alt_val );  

char* getCODStr( ClassAd* ad, const char* id, const char* attr,
				 const char* alt );  

#endif//__TOTALS_H__
