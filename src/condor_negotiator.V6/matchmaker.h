#ifndef __MATCHMAKER_H__
#define __MATCHMAKER_H__

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_accountant.h"

class Matchmaker : public Service
{
	public:
		// ctor/dtor
		Matchmaker();
		~Matchmaker();

		// initialization method (registers command handlers, etc)
		void initialize ();

		// reinitialization method (reconfig)
		int reinitialize ();	

		// command handlers
		int RESCHEDULE_commandHandler (int, Stream*);
		int SET_PRIORITY_commandHandler(int, Stream*);

		// timeout handler (for periodic negotiations)
		int negotiationTime ();

	private:
		// auxillary functions
		bool obtainAdsFromCollector (ClassAdList&, ClassAdList&, ClassAdList&);	
		int  negotiate(char*, char*, double, int, ClassAdList&, ClassAdList&);
				
		ClassAd *matchmakingAlgorithm(ClassAd &, ClassAdList &, double = -1.0);
		int  matchmakingProtocol (ClassAd &, ClassAd *, ClassAdList &, Sock &);
		void calculateNormalizationFactor (ClassAdList &, double &, double &);
		char *getCapability (char *, ClassAdList &);
		friend static int comparisonFunction (ClassAd *, ClassAd *, void *);

		// configuration information
		char *AccountantHost;		// who (if at all) is the accountant?
		int  NegotiatorInterval;	// interval between negotiation cycles
		int  NegotiatorTimeout;		// timeouts for communication
		ExprTree *PreemptionHold;	// do not preempt if this expr evals to true
		
		// rank condition on matches
		ExprTree *rankCondStd;;		// non-preemption (Rank > CurrentRank)
		ExprTree *rankCondPreempt;;	// preemption (Rank >= CurrentRank)

		// the accounting subsystem
		Accountant accountant;
};


#endif//__MATCHMAKER_H__
