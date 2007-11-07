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
#include "HashTable.h"
#include "MyString.h"
#include "status_types.h"

// object keeps track of totals within a single class (same key)
// (i.e., "OpSys/Arch" for a startd, "Name" for schedd, etc.)
class ClassTotal
{
  	public:
		ClassTotal ();
		virtual ~ClassTotal ();

		static ClassTotal *makeTotalObject(ppOption);
		static int makeKey( MyString &, ClassAd *, ppOption);

		virtual int update(ClassAd*) 	= 0;
		virtual void displayHeader(FILE*)= 0;

		// param is zero for non-final totals
		virtual void displayInfo(FILE*, int=0)	= 0;

  	protected:
		ppOption ppo;
};


// object manages totals for different classes within the same ppo
// (e.g., different "Arch/OpSys" in startd normal mode)
class TrackTotals
{
	public:
		TrackTotals (ppOption);
		~TrackTotals();

		int  update(ClassAd *);
		void displayTotals(FILE *, int keyLength);

	private:
		ppOption ppo;
		int malformed;
		HashTable<MyString,ClassTotal*> allTotals;
		ClassTotal*	topLevelTotal;
};



// startd totals
class StartdNormalTotal : public ClassTotal
{
	public:
		StartdNormalTotal();
		virtual int update (ClassAd *);
		virtual void displayHeader(FILE *);
		virtual void displayInfo(FILE *, int);

  	protected:
		int machines;
		int owner;
		int unclaimed;
		int claimed;
		int matched;
		int preempting;
#if HAVE_BACKFILL
		int backfill;
#endif
};


class StartdRunTotal : public ClassTotal
{
	public:
		StartdRunTotal();
		virtual int update (ClassAd *);
		virtual void displayHeader(FILE *);
		virtual void displayInfo(FILE *,int);

	protected:
		int machines;
		long condor_mips;
		long kflops;
		float loadavg;
};


class StartdServerTotal : public ClassTotal
{
	public:
		StartdServerTotal();
		virtual int update (ClassAd *);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

  	protected:
		int machines;
		int avail;
		long memory;
		uint64_t disk;
		long condor_mips;
		long kflops;
};


class StartdStateTotal : public ClassTotal
{
	public:
		StartdStateTotal();
		virtual int update (ClassAd *);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

  	protected:
		int machines;
		int owner;
		int unclaimed;
		int claimed;
		int preempt;
		int matched;
#if HAVE_BACKFILL
		int backfill;
#endif
};



class StartdCODTotal : public ClassTotal
{
public:
	StartdCODTotal();
	virtual int update (ClassAd *);
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


// quill totals
class QuillNormalTotal : public ClassTotal
{
	public:
		QuillNormalTotal();
		virtual int update (ClassAd *);
		virtual void displayHeader(FILE*);
		virtual void displayInfo(FILE*,int);

	protected:
		int numSqlTotal;
		int numSqlLastBatch;
};

// schedd totals
class ScheddNormalTotal : public ClassTotal
{
	public:
		ScheddNormalTotal();
		virtual int update (ClassAd *);
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
		virtual int update (ClassAd *);
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
		virtual int update (ClassAd *);
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
