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
#include "condor_classad.h"
#include "condor_attributes.h"
#include "condor_config.h"
#include "user_job_policy.h"
#include "proc.h"
#include "condor_holdcodes.h"

const char * ATTR_SCRATCH_EXPRESSION = "UserJobPolicyScratchExpression";
const char * PARAM_SYSTEM_PERIODIC_REMOVE = "SYSTEM_PERIODIC_REMOVE";
const char * PARAM_SYSTEM_PERIODIC_RELEASE = "SYSTEM_PERIODIC_RELEASE";
const char * PARAM_SYSTEM_PERIODIC_HOLD = "SYSTEM_PERIODIC_HOLD";

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
	bool on_exit_hold = false, on_exit_remove = false;
	int cdate = 0;
	int adkind;
	
	if (jad == NULL)
	{
		EXCEPT( "Could not evaluate user policy due to job ad being NULL!" );
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
				ExprTree *ph_expr = jad->LookupExpr(ATTR_PERIODIC_HOLD_CHECK);
				ExprTree *pr_expr = jad->LookupExpr(ATTR_PERIODIC_REMOVE_CHECK);
				ExprTree *pl_expr = jad->LookupExpr(ATTR_PERIODIC_RELEASE_CHECK);
				ExprTree *oeh_expr = jad->LookupExpr(ATTR_ON_EXIT_HOLD_CHECK);
				ExprTree *oer_expr = jad->LookupExpr(ATTR_ON_EXIT_REMOVE_CHECK);

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
		{
			/*	The user_policy is checked in this
				order. The first one to succeed is the winner:
		
				periodic_hold
				periodic_exit
				on_exit_hold
				on_exit_remove
			*/

			UserPolicy userpolicy;
#ifdef USE_NON_MUTATING_USERPOLICY
			userpolicy.Init();
			int analyze_result = userpolicy.AnalyzePolicy(*jad, PERIODIC_ONLY);
#else
			userpolicy.Init(jad);
			int analyze_result = userpolicy.AnalyzePolicy(PERIODIC_ONLY);
#endif

			/* should I perform a periodic hold? */
			if(analyze_result == HOLD_IN_QUEUE)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, HOLD_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					userpolicy.FiringExpression());
				result->Insert(buf);

				return result;
			}

			/* Should I perform a periodic remove? */
			if(analyze_result == REMOVE_FROM_QUEUE)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					userpolicy.FiringExpression());
				result->Insert(buf);

				return result;
			}

			/* Should I perform a periodic release? */
			if(analyze_result == RELEASE_FROM_HOLD)
			{
				/* make a result classad explaining this and return it */

				sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
				result->Insert(buf);
				sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
				result->Insert(buf);
				sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
					userpolicy.FiringExpression());
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
			if (jad->LookupExpr(ATTR_ON_EXIT_CODE) == 0 && 
				jad->LookupExpr(ATTR_ON_EXIT_SIGNAL) == 0)
			{
				return result;
			}

			/* Should I hold on exit? */
			jad->LookupBool(ATTR_ON_EXIT_HOLD_CHECK, on_exit_hold);
			if (on_exit_hold)
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
			jad->LookupBool(ATTR_ON_EXIT_REMOVE_CHECK, on_exit_remove);
			if (on_exit_remove)
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
		}

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
	if (attr_expr == NULL)
	{
		dprintf(mode, "%s = UNDEFINED\n", attr);
	}
	else
	{
		dprintf(mode, "%s = %s\n", attr, ExprTreeToString(attr_expr));
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
	ExprTree *ph_expr = suspect->LookupExpr(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = suspect->LookupExpr(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *pl_expr = suspect->LookupExpr(ATTR_PERIODIC_RELEASE_CHECK);
	ExprTree *oeh_expr = suspect->LookupExpr(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = suspect->LookupExpr(ATTR_ON_EXIT_REMOVE_CHECK);

	/* check to see if it is oldstyle */
	if (ph_expr == NULL && pr_expr == NULL && pl_expr == NULL && oeh_expr == NULL && 
		oer_expr == NULL)
	{
		/* check to see if it has ATTR_COMPLETION_DATE, if so then it is
			an oldstyle jobad. If not, it isn't a job ad at all. */

		if (suspect->LookupInteger(ATTR_COMPLETION_DATE, cdate) == 1)
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

#ifdef USE_NON_MUTATING_USERPOLICY
  #define POLICY_NONE                    SYS_POLICY_NONE
  #define POLICY_SYSTEM_PERIODIC_HOLD    SYS_POLICY_PERIODIC_HOLD
  #define POLICY_SYSTEM_PERIODIC_RELEASE SYS_POLICY_PERIODIC_RELEASE
  #define POLICY_SYSTEM_PERIODIC_REMOVE  SYS_POLICY_PERIODIC_REMOVE
#else
  #define POLICY_NONE                    NULL
  #define POLICY_SYSTEM_PERIODIC_HOLD    PARAM_SYSTEM_PERIODIC_HOLD
  #define POLICY_SYSTEM_PERIODIC_RELEASE PARAM_SYSTEM_PERIODIC_RELEASE
  #define POLICY_SYSTEM_PERIODIC_REMOVE  PARAM_SYSTEM_PERIODIC_REMOVE
#endif


UserPolicy::UserPolicy()
#ifdef USE_NON_MUTATING_USERPOLICY
	: m_sys_periodic_hold(NULL)
	, m_sys_periodic_release(NULL)
	, m_sys_periodic_remove(NULL)
	, m_fire_subcode(0)
#else
	: m_ad(NULL)
#endif
	, m_fire_expr_val(-1)
	, m_fire_source(FS_NotYet)
	, m_fire_expr(NULL)
{
}

UserPolicy::~UserPolicy()
{
#ifdef USE_NON_MUTATING_USERPOLICY
	ClearConfig();
#else
	m_ad = NULL;
#endif
	m_fire_expr = NULL;
}

#ifdef USE_NON_MUTATING_USERPOLICY

void UserPolicy::Init()
{
	ResetTriggers();
	Config();
}

void UserPolicy::ClearConfig()
{
	delete m_sys_periodic_hold; m_sys_periodic_hold = NULL;
	delete m_sys_periodic_release; m_sys_periodic_release = NULL;
	delete m_sys_periodic_remove; m_sys_periodic_remove = NULL;
}


void UserPolicy::Config()
{
	ClearConfig();

	auto_free_ptr expr_string(param(PARAM_SYSTEM_PERIODIC_HOLD));
	if (expr_string) {
		ParseClassAdRvalExpr(expr_string, m_sys_periodic_hold);
		long long ival = 1;
		if (m_sys_periodic_hold && ExprTreeIsLiteralNumber(m_sys_periodic_hold, ival) &&  ! ival) {
			delete m_sys_periodic_hold; m_sys_periodic_hold = NULL;
		}
	}
	expr_string.set(param(PARAM_SYSTEM_PERIODIC_RELEASE));
	if (expr_string) {
		ParseClassAdRvalExpr(expr_string, m_sys_periodic_release);
		long long ival = 1;
		if (m_sys_periodic_release && ExprTreeIsLiteralNumber(m_sys_periodic_release, ival) &&  ! ival) {
			delete m_sys_periodic_release; m_sys_periodic_release = NULL;
		}
	}

	expr_string.set(param(PARAM_SYSTEM_PERIODIC_REMOVE));
	if (expr_string) {
		ParseClassAdRvalExpr(expr_string, m_sys_periodic_remove);
		long long ival = 1;
		if (m_sys_periodic_remove && ExprTreeIsLiteralNumber(m_sys_periodic_remove, ival) &&  ! ival) {
			delete m_sys_periodic_remove; m_sys_periodic_remove = NULL;
		}
	}
}

void UserPolicy::ResetTriggers()
{
	m_fire_expr_val = -1;
	m_fire_source = FS_NotYet;
	m_fire_expr = NULL;
}

#else

void UserPolicy::Init(ClassAd *ad)
{
	ASSERT(ad);
	
	m_ad = ad;
	m_fire_expr = NULL;
	m_fire_expr_val = -1;

	this->SetDefaults();
}

void UserPolicy::SetDefaults()
{
	MyString buf;

	ExprTree *ph_expr = m_ad->LookupExpr(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = m_ad->LookupExpr(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *pl_expr = m_ad->LookupExpr(ATTR_PERIODIC_RELEASE_CHECK);
	ExprTree *oeh_expr = m_ad->LookupExpr(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = m_ad->LookupExpr(ATTR_ON_EXIT_REMOVE_CHECK);

	/* if the default user policy expressions do not exist, then add them
		here and now with the usual defaults */
	if (ph_expr == NULL) {
		buf.formatstr( "%s = FALSE", ATTR_PERIODIC_HOLD_CHECK );
		m_ad->Insert( buf.Value() );
	}

	if (pr_expr == NULL) {
		buf.formatstr( "%s = FALSE", ATTR_PERIODIC_REMOVE_CHECK );
		m_ad->Insert( buf.Value() );
	}

	if (pl_expr == NULL) {
		buf.formatstr( "%s = FALSE", ATTR_PERIODIC_RELEASE_CHECK );
		m_ad->Insert( buf.Value() );
	}

	if (oeh_expr == NULL) {
		buf.formatstr( "%s = FALSE", ATTR_ON_EXIT_HOLD_CHECK );
		m_ad->Insert( buf.Value() );
	}

	if (oer_expr == NULL) {
		buf.formatstr( "%s = TRUE", ATTR_ON_EXIT_REMOVE_CHECK );
		m_ad->Insert( buf.Value() );
	}
}

#endif

#ifdef USE_NON_MUTATING_USERPOLICY
int
UserPolicy::AnalyzePolicy(ClassAd & ad, int mode)
{
#else
int
UserPolicy::AnalyzePolicy( int mode )
{
	if (m_ad == NULL)
	{
		EXCEPT("UserPolicy Error: Must call Init() first!");
	}

	ClassAd & ad = *m_ad;
#endif

	int timer_remove;
	int state;

	if (mode != PERIODIC_ONLY && mode != PERIODIC_THEN_EXIT)
	{
		EXCEPT("UserPolicy Error: Unknown mode in AnalyzePolicy()");
	}

	if( ! ad.LookupInteger(ATTR_JOB_STATUS,state) ) {
		return UNDEFINED_EVAL;
	}

		// Clear out our stateful variables
	m_fire_expr = NULL;
	m_fire_expr_val = -1;
	m_fire_unparsed_expr.clear();

	/*	The user_policy is checked in this
			order. The first one to succeed is the winner:
	
			ATTR_TIMER_REMOVE_CHECK
			ATTR_PERIODIC_HOLD_CHECK
			ATTR_PERIODIC_RELEASE_CHECK
			ATTR_PERIODIC_REMOVE_CHECK
			ATTR_ON_EXIT_HOLD_CHECK
			ATTR_ON_EXIT_REMOVE_CHECK
	*/

	/* Should I perform a remove based on the epoch time? */
	m_fire_expr = ATTR_TIMER_REMOVE_CHECK;
	if ( ! ad.LookupInteger(ATTR_TIMER_REMOVE_CHECK, timer_remove)) {
		//check if attribute exists, but is undefined
		ExprTree * expr = ad.Lookup(ATTR_TIMER_REMOVE_CHECK);
		if (expr != NULL)
		{
			m_fire_expr_val = -1;
			m_fire_source = FS_JobAttribute;
		#ifdef USE_NON_MUTATING_USERPOLICY
			ExprTreeToString(expr, m_fire_unparsed_expr);
		#endif
			return UNDEFINED_EVAL;
		}
		timer_remove = -1;
	}
	if( timer_remove >= 0 && timer_remove < time(NULL) ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
	#ifdef USE_NON_MUTATING_USERPOLICY
		ExprTreeToString(ad.Lookup(ATTR_TIMER_REMOVE_CHECK), m_fire_unparsed_expr);
	#endif
		return REMOVE_FROM_QUEUE;
	}

	int retval;

	/* should I perform a periodic hold? */
	if(state!=HELD) {
		if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_HOLD_CHECK, POLICY_SYSTEM_PERIODIC_HOLD, HOLD_IN_QUEUE, retval)) {
			return retval;
		}
	}

	/* Should I perform a periodic release? */
	if(state==HELD) {
		if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_RELEASE_CHECK, POLICY_SYSTEM_PERIODIC_RELEASE, RELEASE_FROM_HOLD, retval)) {
			return retval;
		}
	}

	/* Should I perform a periodic remove? */
	if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_REMOVE_CHECK, POLICY_SYSTEM_PERIODIC_REMOVE, REMOVE_FROM_QUEUE, retval)) {
		return retval;
	}

	if( mode == PERIODIC_ONLY ) {
			// Nothing left to do, just return the default
		m_fire_expr = NULL;
		return STAYS_IN_QUEUE;
	}

	/* else it is PERIODIC_THEN_EXIT so keep going */

	/* This better be in the classad because it determines how the process
		exited, either by signal, or by exit() */
	if( ! ad.LookupExpr(ATTR_ON_EXIT_BY_SIGNAL) ) {
		EXCEPT( "UserPolicy Error: %s is not present in the classad",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

	/* Check to see if ExitSignal or ExitCode 
		are defined, if not, then except because
		caller should have filled this in if calling
		this function saying to check the exit policies. */
	if( ad.LookupExpr(ATTR_ON_EXIT_CODE) == 0 && 
		ad.LookupExpr(ATTR_ON_EXIT_SIGNAL) == 0 )
	{
		EXCEPT( "UserPolicy Error: No signal/exit codes in job ad!" );
	}

	/* Should I hold on exit? */
#if 1
	if (AnalyzeSinglePeriodicPolicy(ad, ATTR_ON_EXIT_HOLD_CHECK, POLICY_NONE, HOLD_IN_QUEUE, retval)) {
		return retval;
	}
#else
	bool on_exit_hold, on_exit_remove;
	m_fire_expr = ATTR_ON_EXIT_HOLD_CHECK;
	if( ! ad.EvalBool(ATTR_ON_EXIT_HOLD_CHECK, m_ad, on_exit_hold) ) {
		m_fire_source = FS_JobAttribute;
		return UNDEFINED_EVAL;
	}
	if( on_exit_hold ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		return HOLD_IN_QUEUE;
	}
#endif

	/* Should I remove on exit? */
#if 1
	ExprTree * expr = ad.Lookup(ATTR_ON_EXIT_REMOVE_CHECK);
	if ( ! expr) {
		// If no expression, the default behavior is to remove on exit.
		m_fire_expr_val = 1; 
		m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
		m_fire_source = FS_JobAttribute;
	#ifdef USE_NON_MUTATING_USERPOLICY
		m_fire_reason.clear();
		m_fire_unparsed_expr = "true";
	#endif
		return REMOVE_FROM_QUEUE;
	}
	if (AnalyzeSinglePeriodicPolicy(ad, ATTR_ON_EXIT_REMOVE_CHECK, POLICY_NONE, REMOVE_FROM_QUEUE, retval)) {
		return retval;
	}
#ifdef USE_NON_MUTATING_USERPOLICY
	ExprTreeToString(expr, m_fire_unparsed_expr);
#endif
#else
	m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
	if( ! m_ad->EvalBool(ATTR_ON_EXIT_REMOVE_CHECK, m_ad, on_exit_remove) ) {
		m_fire_source = FS_JobAttribute;
		return UNDEFINED_EVAL;
	}
	if( on_exit_remove ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		return REMOVE_FROM_QUEUE;
	}
#endif
		// If we didn't want to remove it, OnExitRemove was false,
		// which means we want the job to stay in the queue...
	m_fire_expr_val = 0;
	m_fire_source = FS_JobAttribute;
	return STAYS_IN_QUEUE;
}

#ifdef USE_NON_MUTATING_USERPOLICY

bool UserPolicy::AnalyzeSinglePeriodicPolicy(ClassAd & ad, ExprTree * expr, int on_true_return, int & retval)
{
	ASSERT(expr);

	int result = 0;
	long long ival = 0;

	classad::Value val;
	if (ad.EvaluateExpr(expr, val) && val.IsNumber(ival)) {
		result = (ival != 0);
	} else {
		if (ExprTreeIsLiteral(expr, val) && val.IsUndefinedValue()) {
			// if the expr is defined to be undefined, treat that a false result.
			result = 0;
		} else {
			// if the expr evaluates to undefined, treat that as a triggering expression.
			m_fire_expr_val = -1;
			retval = UNDEFINED_EVAL;
			return true;
		}
	}

	if( result ) {
		m_fire_expr_val = 1;
		retval = on_true_return;
		return true;
	}

	return false;
}

bool UserPolicy::AnalyzeSinglePeriodicPolicy(ClassAd & ad, const char * attrname, SysPolicyId sys_policy, int on_true_return, int & retval)
{
	ASSERT(attrname);

	// Evaluate the specified expression in the job ad
	m_fire_expr = attrname;
	ExprTree * expr = ad.Lookup(attrname);
	if (expr && AnalyzeSinglePeriodicPolicy(ad, expr, on_true_return, retval)) {
		m_fire_source = FS_JobAttribute;
		m_fire_reason.clear();
		m_fire_subcode = 0;

		// Save expression, subcode and reason for the FiringReason() method.
		ExprTreeToString(expr, m_fire_unparsed_expr);
		if (m_fire_expr_val != -1) {
			std::string attr(attrname); attr += "SubCode";
			ad.EvaluateAttrNumber(attr, m_fire_subcode);
			attr = m_fire_expr; attr += "Reason";
			ad.EvaluateAttrString(attr, m_fire_reason);
		}
		return true;
	}

	/* the old code.  looks like this always returns if there is no attrname expression, which is probably wrong...
	int result = 0;
	if(!ad.EvalBool(attrname, m_ad, result)) {
        //check to see if the attribute actually exists, or if it's really
        //  undefined.
        //originally this if-statement wasn't here, so when the expression
        //  tied to this attribute had an undefined value in it, e.g.
        //      PeriodicRemove = DoesNotExist > 0
        //  the function would set retval = UNDEFINED_EVAL and return
        //
        //now it can differentiate between the something in the 
        //expression being undefined, and the attribute itself 
        //being undefined.
        if(m_ad->Lookup(attrname) != NULL)
        {
            m_fire_expr_val = -1;
            m_fire_source = FS_JobAttribute;
        }
        retval = UNDEFINED_EVAL;
		return true;
	}
	*/

	const char * policy_name = NULL;
	switch (sys_policy) {
	case POLICY_SYSTEM_PERIODIC_HOLD:
		expr = m_sys_periodic_hold;
		policy_name = PARAM_SYSTEM_PERIODIC_HOLD;
		break;
	case POLICY_SYSTEM_PERIODIC_RELEASE:
		expr = m_sys_periodic_release;
		policy_name = PARAM_SYSTEM_PERIODIC_RELEASE;
		break;
	case POLICY_SYSTEM_PERIODIC_REMOVE:
		expr = m_sys_periodic_remove;
		policy_name = PARAM_SYSTEM_PERIODIC_REMOVE;
		break;
	default:
		expr = NULL;
		break;
	}

	if (expr) {
#if 0 // don't do this, undefined system periodic expressions don't fire
		int result_ok = AnalyzeSinglePeriodicPolicy(ad, expr, on_true_return, retval);
		if (result_ok && m_fire_expr_val) {
#else
		long long ival = 0;
		classad::Value val;
		if (ad.EvaluateExpr(expr, val) && val.IsNumber(ival) && ival != 0) {
			m_fire_expr_val = 1;
#endif
			m_fire_expr = policy_name;
			m_fire_source = FS_SystemMacro;
			m_fire_reason.clear();
			m_fire_subcode = 0;

			retval = on_true_return;

			// fetch the unparsed value of the expression that fired.
			ExprTreeToString(expr, m_fire_unparsed_expr);

			// temp buffer for building _SUBCODE and _REASON param names.
			char param_sub[sizeof("SYSTEM_PERIODIC_RELEASE_SUBCODE")+10];

			std::string expr_string;
			strcpy(param_sub, policy_name); strcat(param_sub, "_SUBCODE");
			if (param(expr_string, param_sub, "") && ! expr_string.empty()) {
				long long ival;
				classad::Value val;
				if (ad.EvaluateExpr(expr_string, val) && val.IsNumber(ival)) {
					m_fire_subcode = (int)ival;
				}
			}

			strcpy(param_sub, policy_name); strcat(param_sub, "_REASON");
			if (param(expr_string, param_sub, "") && ! expr_string.empty()) {
				classad::Value val;
				if (ad.EvaluateExpr(expr_string, val) && val.IsStringValue(m_fire_reason)) {
					// val.IsStringValue will have already set m_fire_reason
				}
			}

			return true;
		}
	}

	/*
	// TODO: parse these macros into exprs outside of the loop.
	if (macroname) {
		param(m_fire_unparsed_expr, macroname, "");
		if ( ! m_fire_unparsed_expr.empty()) {
			// Just temporarily toss the expression into the job ad
			ad.AssignExpr(ATTR_SCRATCH_EXPRESSION, m_fire_unparsed_expr.c_str());
			expr = ad.Lookup(ATTR_SCRATCH_EXPRESSION);
			int result_ok = expr && AnalyzeSinglePeriodicPolicy(ad, expr, on_true_return, retval);
			ad.Delete(ATTR_SCRATCH_EXPRESSION);
			if (result_ok && m_fire_expr_val) {
				m_fire_expr = macroname;
				m_fire_source = FS_SystemMacro;
				m_fire_reason.clear();
				m_fire_subcode = 0;

				retval = on_true_return;

				std::string param_name(macroname); param_name += "_SUBCODE";
				std::string param_expr;
				if (param(param_expr, param_name.c_str(), "") && ! param_expr.empty()) {
					long long ival;
					classad::Value val;
					if (ad.EvaluateExpr(param_expr, val) && val.IsNumber(ival)) {
						m_fire_subcode = (int)ival;
					}
				}
				param_name = m_fire_expr; param_name += "_REASON";
				if (param(param_expr, param_name.c_str(), "") && ! param_expr.empty()) {
					classad::Value val;
					if (ad.EvaluateExpr(param_expr, val) && val.IsStringValue(m_fire_reason)) {
						//
					}
				}

				return true;
			}

			m_fire_unparsed_expr.clear();
		}
	}
	*/

	return false;
}
#else

bool UserPolicy::AnalyzeSinglePeriodicPolicy(ClassAd & /*ad*/, const char * attrname, const char * macroname, int on_true_return, int & retval)
{
	ASSERT(attrname);

	// Evaluate the specified expression in the job ad
	bool result;
	m_fire_expr = attrname;
	if(!m_ad->EvalBool(attrname, m_ad, result)) {
        //check to see if the attribute actually exists, or if it's really
        //  undefined.
        //originally this if-statement wasn't here, so when the expression
        //  tied to this attribute had an undefined value in it, e.g.
        //      PeriodicRemove = DoesNotExist > 0
        //  the function would set retval = UNDEFINED_EVAL and return
        //
        //now it can differentiate between the something in the 
        //expression being undefined, and the attribute itself 
        //being undefined.
        if(m_ad->Lookup(attrname) != NULL)
        {
            m_fire_expr_val = -1;
            m_fire_source = FS_JobAttribute;
        }
        retval = UNDEFINED_EVAL;
		return true;
	}
	if( result ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		retval = on_true_return;
		return true;
	}
	if(macroname) {
		char * sysexpr = param(macroname);
		if(sysexpr && sysexpr[0]) {
			// Just temporarily toss the expression into the job ad
			m_ad->AssignExpr(ATTR_SCRATCH_EXPRESSION, sysexpr);
			free(sysexpr);
			sysexpr = NULL;
			int result_ok = m_ad->EvalBool(ATTR_SCRATCH_EXPRESSION, m_ad, result);
			m_ad->Delete(ATTR_SCRATCH_EXPRESSION);
			if(result_ok && result) {
				m_fire_expr = macroname;
				m_fire_expr_val = 1;
				m_fire_source = FS_SystemMacro;
				retval = on_true_return;
				return true;
			}
		}
		free(sysexpr);
	}

	return false;

	//MyString macroname = "system_";
	//macroname += attrname;
}

#endif

const char* UserPolicy::FiringExpression(void)
{
	return m_fire_expr;
}

bool UserPolicy::FiringReason(MyString &reason,int &reason_code,int &reason_subcode)
{
	reason_code = 0;
	reason_subcode = 0;

	if ( m_fire_expr == NULL ) {
		return false;
	}

	reason = "";

	const char * expr_src;
#ifdef USE_NON_MUTATING_USERPOLICY
	std::string exprString;
#else
	MyString exprString;
	std::string reason_expr_param;
	std::string reason_expr_attr;
	std::string subcode_expr_param;
	std::string subcode_expr_attr;
#endif
	switch(m_fire_source) {
		case FS_NotYet:
			expr_src = "UNKNOWN (never set)";
			break;

		case FS_JobAttribute:
		{
			expr_src = "job attribute";
#ifdef USE_NON_MUTATING_USERPOLICY
			exprString = m_fire_unparsed_expr.c_str();
			if (m_fire_expr_val == -1) {
				reason_code = CONDOR_HOLD_CODE_JobPolicyUndefined;
			} else {
				reason_code = CONDOR_HOLD_CODE_JobPolicy;
				reason_subcode = m_fire_subcode;
				reason = m_fire_reason;
			}
#else
			ExprTree *tree;
			tree = m_ad->LookupExpr( m_fire_expr );

			// Get a formatted expression string
			if( tree ) {
				exprString = ExprTreeToString( tree );
			}
			if( m_fire_expr_val == -1 ) {
				reason_code = CONDOR_HOLD_CODE_JobPolicyUndefined;
			}
			else {
				reason_code = CONDOR_HOLD_CODE_JobPolicy;
				formatstr(reason_expr_attr,"%sReason", m_fire_expr);
				formatstr(subcode_expr_attr,"%sSubCode", m_fire_expr);
			}
#endif
			break;
		}

		case FS_SystemMacro:
		{
			expr_src = "system macro";
#ifdef USE_NON_MUTATING_USERPOLICY
			exprString = m_fire_unparsed_expr.c_str();
			if( m_fire_expr_val == -1 ) {
				reason_code = CONDOR_HOLD_CODE_SystemPolicyUndefined;
			}
			else {
				reason_code = CONDOR_HOLD_CODE_SystemPolicy;
				reason_subcode = m_fire_subcode;
				reason = m_fire_reason;
			}
#else
			char * val = param(m_fire_expr);
			exprString = val;
			free(val);
			if( m_fire_expr_val == -1 ) {
				reason_code = CONDOR_HOLD_CODE_SystemPolicyUndefined;
			}
			else {
				reason_code = CONDOR_HOLD_CODE_SystemPolicy;
				formatstr(reason_expr_param,"%s_REASON", m_fire_expr);
				formatstr(subcode_expr_param,"%s_SUBCODE", m_fire_expr);
			}
#endif
			break;
		}

		default:
			expr_src = "UNKNOWN (bad value)";
			break;
	}


#ifdef USE_NON_MUTATING_USERPOLICY
	// don't need this. it's handled above.
#else
	MyString subcode_expr;
	if( !subcode_expr_param.empty() &&
		param(subcode_expr,subcode_expr_param.c_str(),NULL) &&
		!subcode_expr.IsEmpty())
	{
		m_ad->AssignExpr(ATTR_SCRATCH_EXPRESSION, subcode_expr.Value());
		m_ad->LookupInteger(ATTR_SCRATCH_EXPRESSION, reason_subcode);
		m_ad->Delete(ATTR_SCRATCH_EXPRESSION);
	}
	else if( !subcode_expr_attr.empty() )
	{
		m_ad->LookupInteger(subcode_expr_attr.c_str(), reason_subcode);
	}

	MyString reason_expr;
	if( !reason_expr_param.empty() &&
		param(reason_expr,reason_expr_param.c_str(),NULL) &&
		!reason_expr.IsEmpty())
	{
		m_ad->AssignExpr(ATTR_SCRATCH_EXPRESSION, reason_expr.Value());
		m_ad->LookupString(ATTR_SCRATCH_EXPRESSION, reason);
		m_ad->Delete(ATTR_SCRATCH_EXPRESSION);
	}
	else if( !reason_expr_attr.empty() )
	{
		m_ad->LookupString(reason_expr_attr.c_str(), reason);
	}
#endif

	if( !reason.empty() ) {
		return true;
	}

	// Format up the reason string
	reason.formatstr( "The %s %s expression '%s' evaluated to ",
					expr_src,
					m_fire_expr,
					exprString.c_str());

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

	return true;
}
