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
		int condor_mips;
		int kflops;
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
		int memory;
		int disk;
		int condor_mips;
		int kflops;
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
		int disk;
};


#endif//__TOTALS_H__
