/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
#include "baseshadow.h"
#include "shadow_user_policy.h"
#include "MyString.h"


ShadowUserPolicy::ShadowUserPolicy() 
{
	tid = -1;
	job_ad = NULL;
	shadow = NULL;

}


ShadowUserPolicy::~ShadowUserPolicy() 
{
	cancelTimer();
		// Don't touch the memory for the job_ad and shadow, since
		// we're not responsible for that
}


void
ShadowUserPolicy::init( ClassAd* job_ad_ptr, BaseShadow* shadow_ptr )
{
	job_ad = job_ad_ptr;
	shadow = shadow_ptr;
	user_policy.Init( job_ad );
}


void
ShadowUserPolicy::cancelTimer( void )
{
	if( tid != -1 ) {
		daemonCore->Cancel_Timer( tid );
		tid = -1;
	}
}


void
ShadowUserPolicy::startTimer( void )
{
		// first, make sure we don't already have a timer running
	cancelTimer();

		// now, register a periodic timer to evaluate the user policy
	tid = daemonCore->
		Register_Timer(20, 20, (TimerHandlercpp)
					   &ShadowUserPolicy::checkPeriodic,
					   "checkPeriodic", this );
	if( tid < 0 ) {
        EXCEPT( "Can't register DC timer!" );
    }
}


void
ShadowUserPolicy::checkAtExit( void )
{
	int action = user_policy.AnalyzePolicy( PERIODIC_THEN_EXIT );

		// All we have to do now is perform the appropriate action.
		// Since this is all shared code w/ the periodic case, we just
		// call a helper function to do the real work.  This helper
		// will not return... it'll eventually result in a DC_Exit(). 
	doAction( action, false );
}


void
ShadowUserPolicy::checkPeriodic( void )
{
	int action = user_policy.AnalyzePolicy( PERIODIC_ONLY );

	if( action == STAYS_IN_QUEUE ) {
			// at periodic evaluations, this is the normal case: the
			// job should stay in the queue.  so, there's nothing
			// special to do, we'll just return now.
		return;
	}
	
		// if we're supposed to do anything else with the job, we
		// need to perform some actions now, so call our helper:
	doAction( action, true );
}


void
ShadowUserPolicy::doAction( int action, bool is_periodic ) 
{
	const char* firing_expr = user_policy.FiringExpression();
	char* tmp = NULL;
	ExprTree *tree, *rhs = NULL;

	tree = job_ad->Lookup( firing_expr );
	if( tree && (rhs=tree->RArg()) ) {
			// cool, we've found the right expression.  let's print it
			// out to a string so we can use it.
		rhs->PrintToNewStr( &tmp );
	} else {
			// this is really strange, we think the firing expr went
			// off, but now we can't look it up in the job ad.
		EXCEPT( "ShadowUserPolicy: Can't find %s in job ClassAd\n",
				firing_expr );
	}

	int firing_value = user_policy.FiringExpressionValue();

	MyString reason = "The ";
	reason += firing_expr;
	reason += " expression '";
	reason += tmp;
	free( tmp );

	reason += "' evaluated to ";
	switch( firing_value ) {
	case 0:
		reason += "FALSE";
		break;
	case 1:
		reason += "TRUE";
		break;
	case -1:
		reason += "UNDEFINED";
		break;
	default:
		EXCEPT( "Unrecognized FiringExpressionValue: %d", 
				firing_value ); 
		break;
	}

	switch( action ) {
	case UNDEFINED_EVAL:
			/* 
			   If a given policy expression was undefined, the user
			   screwed something up and they better deal with it.  We
			   don't just want the job to leave the queue...
			*/
		shadow->holdJob( reason.Value() );
		break;

	case REMOVE_FROM_QUEUE:
		if( is_periodic ) {
			shadow->removeJob( reason.Value() );
		} else {
			shadow->terminateJob();
		}
		break;

	case STAYS_IN_QUEUE:
		if( is_periodic ) {
			EXCEPT( "STAYS_IN_QUEUE should never be handled by "
					"periodic doAction()" );
		}
		shadow->requeueJob( reason.Value() );
		break;

	case HOLD_IN_QUEUE:
		shadow->holdJob( reason.Value() );
		break;

	default:
		EXCEPT( "Unknown action (%d) in ShadowUserPolicy::doAction", 
				action );
	}
}

