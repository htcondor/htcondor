/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "user_job_policy.h"

/* If a job ad was pre user policy and it was determined to have exited. */
const char *old_style_exit = "OldStyleExit";

/* This will be one of the job actions defined above */
const char ATTR_USER_POLICY_ACTION [] = "UserPolicyAction"; 

/* This is one of: ATTR_PERIODIC_HOLD_CHECK, ATTR_PERIODIC_REMOVE_CHECK,
	ATTR_ON_EXIT_HOLD_CHECK, ATTR_ON_EXIT_REMOVE_CHECK, or
	old_style_exit. It allows killer output of what happened and why. And
	since it is defined in terms of other expressions, makes it easy
	to compare against. */
const char ATTR_USER_POLICY_FIRING_EXPR [] = "UserPolicyFiringExpr";

/* true or false, true if it is determined the job should be held or removed
	from the queue. If false, then the caller should put this job back into
	the idle state and undefine these attributes in the job ad:
	ATTR_ON_EXIT_CODE, ATTR_ON_EXIT_SIGNAL, and then change this attribute
	ATTR_ON_EXIT_BY_SIGNAL to false in the job ad. */
const char ATTR_TAKE_ACTION [] = "TakeAction";

/* If there was an error in determining the policy, this will be true. */
const char ATTR_USER_POLICY_ERROR [] = "UserPolicyError";

/* an "errno" of sorts as to why the error happened. */
const char ATTR_USER_ERROR_REASON [] = "ErrorReason";

/* This function determines what should be done with a job given the user
	policy specifed in the job ad. If no policy is specified, then a classad
	is returned detailing that nothing should be done because there wasn't
	a user policy. You are responsible for freeing the classad you get back
	from this function. It can be used in a periodic fashion on job ads
	and has the notion of "nothing should be done to this job" present
	in the classad result you get back. */
ClassAd* user_job_policy(ClassAd *jad)
{
	ClassAd *result;
	char buf[4096]; /* old classads needs to go away */
	int periodic_hold = 0, periodic_remove = 0, periodic_release = 0;
	int on_exit_hold = 0, on_exit_remove = 0;
	int cdate = 0;
	int adkind;
	
	if (jad == NULL)
	{
		EXCEPT( "Could not evaluate user policy due to job ad being NULL!\n" );
	}

	/* Set up the default response of do nothing. The caller should
		just check for this attribute and ATTR_USER_POLICY_ERROR and
		do nothing to the rest of the classad of ATTR_TAKE_ACTION
		is false. */
	result = new ClassAd;
	if (result == NULL)
	{
		EXCEPT("Out of memory!"); /* XXX should this be here? */
	}
	sprintf(buf, "%s = FALSE", ATTR_TAKE_ACTION);
	result->Insert(buf);
	sprintf(buf, "%s = FALSE", ATTR_USER_POLICY_ERROR);
	result->Insert(buf);

	/* figure out the ad kind and then do something with it */

	adkind = JadKind(jad);

	switch(adkind)
	{
		case USER_ERROR_NOT_JOB_AD:
			dprintf(D_ALWAYS, "user_job_policy(): I have something that "
					"doesn't appear to be a job ad! Ignoring.\n");

			sprintf(buf, "%s = TRUE", ATTR_USER_POLICY_ERROR);
			result->Insert(buf);
			sprintf(buf, "%s = %u", ATTR_USER_ERROR_REASON, 
				USER_ERROR_NOT_JOB_AD);
			result->Insert(buf);

			return result;
			break;

		case USER_ERROR_INCONSISTANT:
			dprintf(D_ALWAYS, "user_job_policy(): Inconsistant jobad state "
								"with respect to user_policy. Detail "
								"follows:\n");
			{
				ExprTree *ph_expr = jad->Lookup(ATTR_PERIODIC_HOLD_CHECK);
				ExprTree *pr_expr = jad->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
				ExprTree *pl_expr = jad->Lookup(ATTR_PERIODIC_RELEASE_CHECK);
				ExprTree *oeh_expr = jad->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
				ExprTree *oer_expr = jad->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

				EmitExpression(D_ALWAYS, ATTR_PERIODIC_HOLD_CHECK, ph_expr);
				EmitExpression(D_ALWAYS, ATTR_PERIODIC_REMOVE_CHECK, pr_expr);
				EmitExpression(D_ALWAYS, ATTR_PERIODIC_RELEASE_CHECK, pl_expr);
				EmitExpression(D_ALWAYS, ATTR_ON_EXIT_HOLD_CHECK, oeh_expr);
				EmitExpression(D_ALWAYS, ATTR_ON_EXIT_REMOVE_CHECK, oer_expr);
			}

			sprintf(buf, "%s = TRUE", ATTR_USER_POLICY_ERROR);
			result->Insert(buf);
			sprintf(buf, "%s = %u", ATTR_USER_ERROR_REASON, 
				USER_ERROR_INCONSISTANT);
			result->Insert(buf);

			return result;
			break;

		case KIND_OLDSTYLE:
			jad->LookupInteger(ATTR_COMPLETION_DATE, cdate);
			if (cdate > 0)
			{
				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					old_style_exit);
				result->Insert(buf);
			}
			return result;
			break;

		case KIND_NEWSTYLE:
			/*	The user_policy is checked in this
				order. The first one to succeed is the winner:
		
				periodic_hold
				periodic_exit
				on_exit_hold
				on_exit_remove
			*/

			/* should I perform a periodic hold? */
			jad->EvalBool(ATTR_PERIODIC_HOLD_CHECK, jad, periodic_hold);
			if (periodic_hold == 1)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, HOLD_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					ATTR_PERIODIC_HOLD_CHECK);
				result->Insert(buf);

				return result;
			}

			/* Should I perform a periodic remove? */
			jad->EvalBool(ATTR_PERIODIC_REMOVE_CHECK, jad, periodic_remove);
			if (periodic_remove == 1)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					ATTR_PERIODIC_REMOVE_CHECK);
				result->Insert(buf);

				return result;
			}

			/* Should I perform a periodic release? */
			jad->EvalBool(ATTR_PERIODIC_RELEASE_CHECK, jad, periodic_release);
			if (periodic_release == 1)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					ATTR_PERIODIC_RELEASE_CHECK);
				result->Insert(buf);

				return result;
			}

			/* Check to see if ExitSignal or ExitCode 
				are defined, if not, then assume the
				job hadn't exited and don't check the
				policy. This could hide a mistake of
				the caller to insert those attributes
				correctly but allows checking of the
				job ad in a periodic context. */
			if (jad->Lookup(ATTR_ON_EXIT_CODE) == 0 && 
				jad->Lookup(ATTR_ON_EXIT_SIGNAL) == 0)
			{
				return result;
			}

			/* Should I hold on exit? */
			jad->EvalBool(ATTR_ON_EXIT_HOLD_CHECK, jad, on_exit_hold);
			if (on_exit_hold == 1)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, HOLD_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					ATTR_ON_EXIT_HOLD_CHECK);
				result->Insert(buf);

				return result;
			}

			/* Should I remove on exit? */
			jad->EvalBool(ATTR_ON_EXIT_REMOVE_CHECK, jad, on_exit_remove);
			if (on_exit_remove == 1)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					ATTR_ON_EXIT_REMOVE_CHECK);
				result->Insert(buf);

				return result;
			}

			/* just return the default of leaving the job in idle state */
			return result;

			break;

		default:
			dprintf(D_ALWAYS, "JadKind() returned unknown ad kind\n");

			/* just return the default of leaving the job in idle state. This
				is safest. */
			return result;

			break;
	}

	/* just return the default of leaving the job in idle state */
	return result;
}

void EmitExpression(unsigned int mode, const char *attr, ExprTree* attr_expr)
{
	char buf[8192]; /* PrintToStr is dumb, hope this is big enough */

	if (attr_expr == NULL)
	{
		dprintf(mode, "%s = UNDEFINED\n", attr);
	}
	else
	{
		attr_expr->PrintToStr(buf);
		dprintf(mode, "%s = %s\n", attr, buf);
	}
}

/* This function takes a classad and forces it to return to the idle state.
	It does this by undefining or resetting certain attributes in the job
	ad to a pre-exited state. It returns a stringlist containing the attributes
	that had been modified in the job ad so you can SetAttribute later
	with the modified attributes. */

/*StringList* force_job_ad_to_idle(ClassAd *jad)*/
/*{*/
	
/*}*/


/* is this classad oldstyle, newstyle, or even a job ad? */
int JadKind(ClassAd *suspect)
{
	int cdate;

	/* determine if I have a user job ad with the new user policy expressions
		enabled. */
	ExprTree *ph_expr = suspect->Lookup(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = suspect->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *pl_expr = suspect->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *oeh_expr = suspect->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = suspect->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

	/* check to see if it is oldstyle */
	if (ph_expr == NULL && pr_expr == NULL && pl_expr == NULL && oeh_expr == NULL && 
		oer_expr == NULL)
	{
		/* check to see if it has ATTR_COMPLETION_DATE, if so then it is
			an oldstyle jobad. If not, it isn't a job ad at all. */

		if (suspect->LookupInteger(ATTR_COMPLETION_DATE, cdate) == 0)
		{
			return KIND_OLDSTYLE;
		}

		return USER_ERROR_NOT_JOB_AD;
	}

	/* check to see if it is a consistant user policy job ad. */
	if (ph_expr == NULL || pr_expr == NULL || pl_expr == NULL || oeh_expr == NULL || 
		oer_expr == NULL)
	{
		return USER_ERROR_INCONSISTANT;
	}
	
	return KIND_NEWSTYLE;
}


/* NEW INTERFACE */


UserPolicy::UserPolicy()
{
	m_ad = NULL;
	m_fire_expr = NULL;
	m_fire_expr_val = -1;
}

UserPolicy::~UserPolicy()
{
	m_ad = NULL;
	m_fire_expr = NULL;
}

void UserPolicy::Init(ClassAd *ad)
{
	m_ad = ad;
	m_fire_expr = NULL;
	m_fire_expr_val = -1;

	this->SetDefaults();
}

void UserPolicy::SetDefaults()
{
	char buf[8192];

	ExprTree *ph_expr = m_ad->Lookup(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = m_ad->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *pl_expr = m_ad->Lookup(ATTR_PERIODIC_RELEASE_CHECK);
	ExprTree *oeh_expr = m_ad->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = m_ad->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

	/* if the default user policy expressions do not exist, then add them
		here and now with the usual defaults */
	if (ph_expr == NULL) {
		sprintf(buf, "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK);
		m_ad->Insert(buf);
	}

	if (pr_expr == NULL) {
		sprintf(buf, "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK);
		m_ad->Insert(buf);
	}

	if (pl_expr == NULL) {
		sprintf(buf, "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK);
		m_ad->Insert(buf);
	}

	if (oeh_expr == NULL) {
		sprintf(buf, "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK);
		m_ad->Insert(buf);
	}

	if (oer_expr == NULL) {
		sprintf(buf, "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK);
		m_ad->Insert(buf);
	}
}

int
UserPolicy::AnalyzePolicy( int mode )
{
	int periodic_hold, periodic_remove, periodic_release;
	int on_exit_hold, on_exit_remove;
	int timer_remove;
	int state;

	if (m_ad == NULL)
	{
		EXCEPT("UserPolicy Error: Must call Init() first!");
	}

	if (mode != PERIODIC_ONLY && mode != PERIODIC_THEN_EXIT)
	{
		EXCEPT("UserPolicy Error: Unknown mode in AnalyzePolicy()");
	}

	if( !m_ad->LookupInteger(ATTR_JOB_STATUS,state) ) {
		return UNDEFINED_EVAL;
	}

		// Clear out our stateful variables
	m_fire_expr = NULL;
	m_fire_expr_val = -1;

	/*	The user_policy is checked in this
			order. The first one to succeed is the winner:
	
			ATTR_TIMER_REMOVE_CHECK
			ATTR_PERIODIC_HOLD_CHECK
			ATTR_PERIODIC_REMOVE_CHECK
			ATTR_ON_EXIT_HOLD_CHECK
			ATTR_ON_EXIT_REMOVE_CHECK
	*/

	/* Should I perform a periodic remove? */
	m_fire_expr = ATTR_TIMER_REMOVE_CHECK;
	if(!m_ad->LookupInteger(ATTR_TIMER_REMOVE_CHECK, timer_remove)) {
		timer_remove = -1;
	}
	if( timer_remove >= 0 && timer_remove < time(NULL) ) {
		m_fire_expr_val = 1;
		return REMOVE_FROM_QUEUE;
	}

	/* should I perform a periodic hold? */
	if(state!=HELD) {
		m_fire_expr = ATTR_PERIODIC_HOLD_CHECK;
		if( ! m_ad->EvalBool(ATTR_PERIODIC_HOLD_CHECK, m_ad, periodic_hold) ) {
			return UNDEFINED_EVAL;
		}
		if( periodic_hold ) {
			m_fire_expr_val = 1;
			return HOLD_IN_QUEUE;
		}
	}

	/* Should I perform a periodic release? */
	if(state==HELD) {
		m_fire_expr = ATTR_PERIODIC_RELEASE_CHECK;
		if(!m_ad->EvalBool(ATTR_PERIODIC_RELEASE_CHECK, m_ad, periodic_release)) {
			return UNDEFINED_EVAL;
		}
		if( periodic_release ) {
			m_fire_expr_val = 1;
			return RELEASE_FROM_HOLD;
		}
	}

	/* Should I perform a periodic remove? */
	m_fire_expr = ATTR_PERIODIC_REMOVE_CHECK;
	if(!m_ad->EvalBool(ATTR_PERIODIC_REMOVE_CHECK, m_ad, periodic_remove)) {
		return UNDEFINED_EVAL;
	}
	if( periodic_remove ) {
		m_fire_expr_val = 1;
		return REMOVE_FROM_QUEUE;
	}

	if( mode == PERIODIC_ONLY ) {
			// Nothing left to do, just return the default
		m_fire_expr = NULL;
		return STAYS_IN_QUEUE;
	}

	/* else it is PERIODIC_THEN_EXIT so keep going */

	/* This better be in the classad because it determines how the process
		exited, either by signal, or by exit() */
	if( ! m_ad->Lookup(ATTR_ON_EXIT_BY_SIGNAL) ) {
		EXCEPT( "UserPolicy Error: %s is not present in the classad",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

	/* Check to see if ExitSignal or ExitCode 
		are defined, if not, then except because
		caller should have filled this in if calling
		this function saying to check the exit policies. */
	if( m_ad->Lookup(ATTR_ON_EXIT_CODE) == 0 && 
		m_ad->Lookup(ATTR_ON_EXIT_SIGNAL) == 0 )
	{
		EXCEPT( "UserPolicy Error: No signal/exit codes in job ad!" );
	}

	/* Should I hold on exit? */
	m_fire_expr = ATTR_ON_EXIT_HOLD_CHECK;
	if( ! m_ad->EvalBool(ATTR_ON_EXIT_HOLD_CHECK, m_ad, on_exit_hold) ) {
		return UNDEFINED_EVAL;
	}
	if( on_exit_hold ) {
		m_fire_expr_val = 1;
		return HOLD_IN_QUEUE;
	}

	/* Should I remove on exit? */
	m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
	if( ! m_ad->EvalBool(ATTR_ON_EXIT_REMOVE_CHECK, m_ad, on_exit_remove) ) {
		return UNDEFINED_EVAL;
	}
	if( on_exit_remove ) {
		m_fire_expr_val = 1;
		return REMOVE_FROM_QUEUE;
	}
		// If we didn't want to remove it, OnExitRemove was false,
		// which means we want the job to stay in the queue...
	m_fire_expr_val = 0;
	return STAYS_IN_QUEUE;
}

const char* UserPolicy::FiringExpression(void)
{
	return m_fire_expr;
}

const char* UserPolicy::FiringReason()
{
	static MyString reason;

	if ( m_ad == NULL || m_fire_expr == NULL ) {
		return NULL;
	}

	ExprTree *tree, *rhs = NULL;
	tree = m_ad->Lookup( m_fire_expr );

	// Get a formatted expression string
	char* exprString = NULL;
	if( tree && (rhs=tree->RArg()) ) {
		rhs->PrintToNewStr( &exprString );
	}

	// Format up the reason string
	reason.sprintf( "The %s expression '%s' evaluated to ",
					m_fire_expr,
					exprString ? exprString : "" );

	// Free up the buffer from PrintToNewStr()
	if ( exprString ) {
		free( exprString );
	}

	// Get a string for it's value
	switch( m_fire_expr_val ) {
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
				m_fire_expr_val ); 
		break;
	}

	return reason.Value();
}









