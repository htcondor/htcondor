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
			userpolicy.Init(jad);
			int analyze_result = userpolicy.AnalyzePolicy(PERIODIC_ONLY);

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


UserPolicy::UserPolicy()
{
	m_ad = NULL;
	m_fire_source = FS_NotYet;
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

int
UserPolicy::AnalyzePolicy( int mode )
{
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
			ATTR_PERIODIC_RELEASE_CHECK
			ATTR_PERIODIC_REMOVE_CHECK
			ATTR_ON_EXIT_HOLD_CHECK
			ATTR_ON_EXIT_REMOVE_CHECK
	*/

	/* Should I perform a remove based on the epoch time? */
	m_fire_expr = ATTR_TIMER_REMOVE_CHECK;
	if(!m_ad->LookupInteger(ATTR_TIMER_REMOVE_CHECK, timer_remove)) {
		//check if attribute exists, but is undefined
		if(m_ad->Lookup(ATTR_TIMER_REMOVE_CHECK) != NULL)
		{
			m_fire_expr_val = -1;
			m_fire_source = FS_JobAttribute;
			return UNDEFINED_EVAL;
		}
		timer_remove = -1;
	}
	if( timer_remove >= 0 && timer_remove < time(NULL) ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		return REMOVE_FROM_QUEUE;
	}

	int retval;

	/* should I perform a periodic hold? */
	if(state!=HELD) {
		if(AnalyzeSinglePeriodicPolicy(ATTR_PERIODIC_HOLD_CHECK, PARAM_SYSTEM_PERIODIC_HOLD, HOLD_IN_QUEUE, retval)) {
			return retval;
		}
	}

	/* Should I perform a periodic release? */
	if(state==HELD) {
		if(AnalyzeSinglePeriodicPolicy(ATTR_PERIODIC_RELEASE_CHECK, PARAM_SYSTEM_PERIODIC_RELEASE, RELEASE_FROM_HOLD, retval)) {
			return retval;
		}
	}

	/* Should I perform a periodic remove? */
	if(AnalyzeSinglePeriodicPolicy(ATTR_PERIODIC_REMOVE_CHECK, PARAM_SYSTEM_PERIODIC_REMOVE, REMOVE_FROM_QUEUE, retval)) {
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
	if( ! m_ad->LookupExpr(ATTR_ON_EXIT_BY_SIGNAL) ) {
		EXCEPT( "UserPolicy Error: %s is not present in the classad",
				ATTR_ON_EXIT_BY_SIGNAL );
	}

	/* Check to see if ExitSignal or ExitCode 
		are defined, if not, then except because
		caller should have filled this in if calling
		this function saying to check the exit policies. */
	if( m_ad->LookupExpr(ATTR_ON_EXIT_CODE) == 0 && 
		m_ad->LookupExpr(ATTR_ON_EXIT_SIGNAL) == 0 )
	{
		EXCEPT( "UserPolicy Error: No signal/exit codes in job ad!" );
	}

	/* Should I hold on exit? */
	m_fire_expr = ATTR_ON_EXIT_HOLD_CHECK;
	if( ! m_ad->EvalBool(ATTR_ON_EXIT_HOLD_CHECK, m_ad, on_exit_hold) ) {
		m_fire_source = FS_JobAttribute;
		return UNDEFINED_EVAL;
	}
	if( on_exit_hold ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		return HOLD_IN_QUEUE;
	}

	/* Should I remove on exit? */
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
		// If we didn't want to remove it, OnExitRemove was false,
		// which means we want the job to stay in the queue...
	m_fire_expr_val = 0;
	m_fire_source = FS_JobAttribute;
	return STAYS_IN_QUEUE;
}


bool UserPolicy::AnalyzeSinglePeriodicPolicy(const char * attrname, const char * macroname, int on_true_return, int & retval)
{
	ASSERT(attrname);

	// Evaluate the specified expression in the job ad
	int result;
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

const char* UserPolicy::FiringExpression(void)
{
	return m_fire_expr;
}

bool UserPolicy::FiringReason(MyString &reason,int &reason_code,int &reason_subcode)
{
	reason_code = 0;
	reason_subcode = 0;

	if ( m_ad == NULL || m_fire_expr == NULL ) {
		return false;
	}


	const char * expr_src;
	MyString exprString;
	std::string reason_expr_param;
	std::string reason_expr_attr;
	std::string subcode_expr_param;
	std::string subcode_expr_attr;
	switch(m_fire_source) {
		case FS_NotYet:
			expr_src = "UNKNOWN (never set)";
			break;

		case FS_JobAttribute:
		{
			expr_src = "job attribute";
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
			break;
		}

		case FS_SystemMacro:
		{
			expr_src = "system macro";
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
			break;
		}

		default:
			expr_src = "UNKNOWN (bad value)";
			break;
	}

	reason = "";

	MyString subcode_expr;
	if( !subcode_expr_param.empty() &&
		param(subcode_expr,subcode_expr_param.c_str(),NULL) &&
		!subcode_expr.IsEmpty())
	{
		m_ad->AssignExpr(ATTR_SCRATCH_EXPRESSION, subcode_expr.Value());
		m_ad->EvalInteger(ATTR_SCRATCH_EXPRESSION, m_ad, reason_subcode);
		m_ad->Delete(ATTR_SCRATCH_EXPRESSION);
	}
	else if( !subcode_expr_attr.empty() )
	{
		m_ad->EvalInteger(subcode_expr_attr.c_str(), m_ad, reason_subcode);
	}

	MyString reason_expr;
	if( !reason_expr_param.empty() &&
		param(reason_expr,reason_expr_param.c_str(),NULL) &&
		!reason_expr.IsEmpty())
	{
		m_ad->AssignExpr(ATTR_SCRATCH_EXPRESSION, reason_expr.Value());
		m_ad->EvalString(ATTR_SCRATCH_EXPRESSION, m_ad, reason);
		m_ad->Delete(ATTR_SCRATCH_EXPRESSION);
	}
	else if( !reason_expr_attr.empty() )
	{
		m_ad->EvalString(reason_expr_attr.c_str(), m_ad, reason);
	}

	if( !reason.IsEmpty() ) {
		return true;
	}

	// Format up the reason string
	reason.formatstr( "The %s %s expression '%s' evaluated to ",
					expr_src,
					m_fire_expr,
					exprString.Value());

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
