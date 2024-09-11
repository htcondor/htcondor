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
#include "format_time.h"

#define PARAM_SYSTEM_PERIODIC_REMOVE "SYSTEM_PERIODIC_REMOVE"
#define PARAM_SYSTEM_PERIODIC_RELEASE "SYSTEM_PERIODIC_RELEASE"
#define PARAM_SYSTEM_PERIODIC_HOLD "SYSTEM_PERIODIC_HOLD"
#define PARAM_SYSTEM_PERIODIC_VACATE "SYSTEM_PERIODIC_VACATE"

#ifdef ENABLE_JOB_POLICY_LISTS

static void load_policy_list(const char * knob_base, std::vector<JobPolicyExpr> & policies)
{
	std::string knob; knob.reserve(32);
	knob = knob_base; knob += "_NAMES";

	std::vector<std::string> items;
	if (param_and_insert_unique_items(knob.c_str(), items)) {
		policies.reserve(items.size()+1);

		for (auto& tag: items) {
			if (YourStringNoCase("NAMES") == tag) continue;

			JobPolicyExpr policy(tag.c_str());
			knob = knob_base; policy.append_tag(knob);
			policy.set_from_config(knob.c_str());

			int parse_err = 0;
			policy.Expr(&parse_err);
			if (parse_err) {
				dprintf(D_ALWAYS, "WARNING: ignoring invalid %s expression : %s\n", knob.c_str(), policy.Str());
				continue;
			}
			if (policy.is_trivial()) continue;
			policies.push_back(policy);
		}
	}

	JobPolicyExpr old_policy;
	old_policy.set_from_config(knob_base);
	if ( ! old_policy.is_trivial()) {
		policies.push_back(old_policy);
	}
}

#else

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

#endif // ENABLE_JOB_POLICY_LISTS

/* NEW INTERFACE */

  #define POLICY_NONE                    SYS_POLICY_NONE
  #define POLICY_SYSTEM_PERIODIC_HOLD    SYS_POLICY_PERIODIC_HOLD
  #define POLICY_SYSTEM_PERIODIC_RELEASE SYS_POLICY_PERIODIC_RELEASE
  #define POLICY_SYSTEM_PERIODIC_REMOVE  SYS_POLICY_PERIODIC_REMOVE
  #define POLICY_SYSTEM_PERIODIC_VACATE  SYS_POLICY_PERIODIC_VACATE


void UserPolicy::Init()
{
	ResetTriggers();
	Config();
}

void UserPolicy::Config()
{
	ClearConfig();

#ifdef ENABLE_JOB_POLICY_LISTS
	load_policy_list(PARAM_SYSTEM_PERIODIC_HOLD, m_sys_periodic_holds);
	load_policy_list(PARAM_SYSTEM_PERIODIC_RELEASE, m_sys_periodic_releases);
	load_policy_list(PARAM_SYSTEM_PERIODIC_REMOVE, m_sys_periodic_removes);
	load_policy_list(PARAM_SYSTEM_PERIODIC_VACATE, m_sys_periodic_vacates);
#else
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
#endif

}

void UserPolicy::ResetTriggers()
{
	m_fire_expr_val = -1;
	m_fire_source = FS_NotYet;
	m_fire_expr = NULL;
}

int
UserPolicy::AnalyzePolicy(ClassAd & ad, int mode, int state)
{

	int timer_remove;

	if (mode != PERIODIC_ONLY && mode != PERIODIC_THEN_EXIT)
	{
		dprintf(D_ERROR, "UserPolicy Error: Unknown mode %d in AnalyzePolicy()\n", mode);
		return UNDEFINED_EVAL;
	}

	if( state < 0 && ! ad.LookupInteger(ATTR_JOB_STATUS, state) ) {
		dprintf( D_ERROR, "UserPolicy Error: %s is not present in the classad\n",
		         ATTR_JOB_STATUS );
		return UNDEFINED_EVAL;
	}

		// Clear out our stateful variables
	m_fire_expr = NULL;
	m_fire_expr_val = -1;
	m_fire_unparsed_expr.clear();

	/*	The user_policy is checked in this
			order. The first one to succeed is the winner:

			ATTR_ALLOWED_JOB_DURATION
			ATTR_TIMER_REMOVE_CHECK
			ATTR_PERIODIC_VACATE_CHECK
			ATTR_PERIODIC_HOLD_CHECK
			ATTR_PERIODIC_RELEASE_CHECK
			ATTR_PERIODIC_REMOVE_CHECK
			ATTR_ON_EXIT_HOLD_CHECK
			ATTR_ON_EXIT_REMOVE_CHECK
	*/

	/* Don't do any policy evaluation on removed jobs.
	 * Act as if no policy expressions were defined.
	 */
	if( state == REMOVED) {
		if( mode == PERIODIC_ONLY ) {
			return STAYS_IN_QUEUE;
		} else {
			m_fire_expr_val = 1;
			m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
			m_fire_source = FS_JobAttribute;
			m_fire_reason.clear();
			m_fire_unparsed_expr = "true";
			return REMOVE_FROM_QUEUE;
		}
	}

	if (state == RUNNING || state == SUSPENDED) {
		int birthday;

		/* Should I perform a hold based on the "running" time of the job? */
		int allowedJobDuration;
		if( ad.LookupInteger( ATTR_JOB_ALLOWED_JOB_DURATION, allowedJobDuration ) ) {
			// Arguably, we should be calling BaseUserPolicy::getJobBirthday()
			// here, but we don't have access to that here.  This will probably
			// cause some confusion in the local universe, because it otherwise
			// uses ATTR_JOB_START_DATE to determine duration, but using
			// ATTR_SHADOW_BIRTHDATE was the assignment and is simpler.
			if( ad.LookupInteger( ATTR_SHADOW_BIRTHDATE, birthday ) ) {
				if( time(NULL) - birthday >= allowedJobDuration ) {
					m_fire_expr = ATTR_JOB_ALLOWED_JOB_DURATION;
					m_fire_source = FS_JobDuration;
					formatstr(m_fire_reason, "The job exceeded allowed job duration of %s", format_time_short(allowedJobDuration));
					return HOLD_IN_QUEUE;
				}
			}
		}

		/* Should I perform a hold based on the "execute" time of the job? */
		/* If ATTR_JOB_CURRENT_START_EXECUTING_DATE is less than
		 * ATTR_SHADOW_BIRTHDATE, then it's left over from a previous
		 * execution attempt.
		 */
		int allowedExecuteDuration, beganExecuting;
		if (ad.LookupInteger(ATTR_JOB_ALLOWED_EXECUTE_DURATION, allowedExecuteDuration) &&
			ad.LookupInteger(ATTR_JOB_CURRENT_START_EXECUTING_DATE, beganExecuting) &&
			ad.LookupInteger(ATTR_SHADOW_BIRTHDATE, birthday) &&
			beganExecuting > birthday) {

			// We use TransferOutFinished because the shadow only sets
			// ATTR_JOB_CURRENT_FINISH_TRANSFER_OUTPUT_DATE at job exit.
			int TransferOutFinished;
			bool tof = ad.LookupInteger("TransferOutFinished", TransferOutFinished);
			bool checkpointed = tof && (TransferOutFinished > beganExecuting);
			if (checkpointed) {
				beganExecuting = TransferOutFinished;
			}

			if ((time(NULL) - beganExecuting) > allowedExecuteDuration) {
				m_fire_expr = ATTR_JOB_ALLOWED_EXECUTE_DURATION;
				m_fire_source = FS_ExecuteDuration;
				formatstr(m_fire_reason, "The job exceeded allowed execute duration of %s", format_time_short(allowedExecuteDuration));
				return HOLD_IN_QUEUE;
			}
		}

		/* Should I perform a periodic vacate? */
		if (mode == PERIODIC_ONLY) {
			int retval = 0;
			if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_VACATE_CHECK, POLICY_SYSTEM_PERIODIC_VACATE, VACATE_FROM_RUNNING, retval)) {
				return retval;
			}
		}
	}

	/* Should I perform a remove based on the epoch time? */
	m_fire_expr = ATTR_TIMER_REMOVE_CHECK;
	if ( ! ad.LookupInteger(ATTR_TIMER_REMOVE_CHECK, timer_remove)) {
		//check if attribute exists, but is undefined
		ExprTree * expr = ad.Lookup(ATTR_TIMER_REMOVE_CHECK);
		if (expr != NULL)
		{
			m_fire_expr_val = -1;
			m_fire_source = FS_JobAttribute;
			ExprTreeToString(expr, m_fire_unparsed_expr);
			return UNDEFINED_EVAL;
		}
		timer_remove = -1;
	}
	if( timer_remove >= 0 && timer_remove < time(NULL) ) {
		m_fire_expr_val = 1;
		m_fire_source = FS_JobAttribute;
		ExprTreeToString(ad.Lookup(ATTR_TIMER_REMOVE_CHECK), m_fire_unparsed_expr);
		return REMOVE_FROM_QUEUE;
	}

	int retval;

	/* should I perform a periodic hold? */
	if(state!=HELD && state!=COMPLETED) {
		if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_HOLD_CHECK, POLICY_SYSTEM_PERIODIC_HOLD, HOLD_IN_QUEUE, retval)) {
			return retval;
		}
	}

	/* Should I perform a periodic release? */
	if(state==HELD) {
		/* Ignore jobs held at user request */
		int hold_code = 0;
		ad.LookupInteger(ATTR_HOLD_REASON_CODE, hold_code);
		if(hold_code != CONDOR_HOLD_CODE::UserRequest) {
			if(AnalyzeSinglePeriodicPolicy(ad, ATTR_PERIODIC_RELEASE_CHECK, POLICY_SYSTEM_PERIODIC_RELEASE, RELEASE_FROM_HOLD, retval)) {
				return retval;
			}
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
		dprintf( D_ERROR, "UserPolicy Error: %s is not present in the classad\n",
		         ATTR_ON_EXIT_BY_SIGNAL );
		return UNDEFINED_EVAL;
	}

	/* Check to see if ExitSignal or ExitCode
		are defined, if not, then except because
		caller should have filled this in if calling
		this function saying to check the exit policies. */
	if( ad.LookupExpr(ATTR_ON_EXIT_CODE) == 0 &&
		ad.LookupExpr(ATTR_ON_EXIT_SIGNAL) == 0 )
	{
		dprintf( D_ERROR, "UserPolicy Error: No signal/exit codes in job ad!\n" );
		return UNDEFINED_EVAL;
	}

	/* Should I hold on exit? */
	if (AnalyzeSinglePeriodicPolicy(ad, ATTR_ON_EXIT_HOLD_CHECK, POLICY_NONE, HOLD_IN_QUEUE, retval)) {
		return retval;
	}

	/* Should I remove on exit? */
	m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
	m_fire_source = FS_JobAttribute;
	m_fire_reason.clear();
	m_fire_subcode = 0;
	ExprTree * expr = ad.Lookup(ATTR_ON_EXIT_REMOVE_CHECK);
	if (expr) {
		classad::Value val;
		if (ad.EvaluateExpr(expr, val) && val.IsNumber(m_fire_expr_val) && m_fire_expr_val == 0) {
			// for backward compatibility, unparse the trigger expression for use in writing
			// the log terminate event.
			ExprTreeToString(expr, m_fire_unparsed_expr);

			// OnExitRemove was false, which means we want the job to stay in the queue...
			return STAYS_IN_QUEUE;
		}
	}

	// no expression, or evaluated to anything but false - remove
	m_fire_expr_val = 1;
	return REMOVE_FROM_QUEUE;
}

bool UserPolicy::AnalyzeSinglePeriodicPolicy(ClassAd & ad, ExprTree * expr, int on_true_return, int & retval)
{
	ASSERT(expr);

	int result = 0;
	long long ival = 0;

	classad::Value val;
	if (ad.EvaluateExpr(expr, val) && val.IsNumber(ival)) {
		result = (ival != 0);
	} else {
#if 0 // we don't want to treat undefined as triggering 
		if (ExprTreeIsLiteral(expr, val) && val.IsUndefinedValue()) {
			// if the expr is defined to be undefined, treat that a false result.
			result = 0;
		} else {
			// if the expr evaluates to undefined, treat that as a triggering expression.
			m_fire_expr_val = -1;
			retval = UNDEFINED_EVAL;
			return true;
		}
#endif
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

#ifdef ENABLE_JOB_POLICY_LISTS // multi policy
	const char * policy_name = NULL;
	std::vector<JobPolicyExpr> * policies = nullptr;
	switch (sys_policy) {
	case POLICY_SYSTEM_PERIODIC_HOLD:
		policies = &m_sys_periodic_holds;
		policy_name = PARAM_SYSTEM_PERIODIC_HOLD;
		break;
	case POLICY_SYSTEM_PERIODIC_RELEASE:
		policies = &m_sys_periodic_releases;
		policy_name = PARAM_SYSTEM_PERIODIC_RELEASE;
		break;
	case POLICY_SYSTEM_PERIODIC_REMOVE:
		policies = &m_sys_periodic_removes;
		policy_name = PARAM_SYSTEM_PERIODIC_REMOVE;
		break;
	default:
		return false;
	}
	for (auto & policy : *policies) {
		// TODO: remove this line once all above are lists
		expr = policy.Expr();
		if (! expr) continue;
#else
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
#endif
		long long ival = 0;
		classad::Value val;
		if (ad.EvaluateExpr(expr, val) && val.IsNumber(ival) && ival != 0) {
			m_fire_expr_val = 1;
			m_fire_expr = policy_name;
			m_fire_source = FS_SystemMacro;
			m_fire_reason.clear();
			m_fire_subcode = 0;

			retval = on_true_return;

		#ifdef ENABLE_JOB_POLICY_LISTS // multi policy
			m_fire_unparsed_expr = policy.Str();
		#else
			// fetch the unparsed value of the expression that fired.
			ExprTreeToString(expr, m_fire_unparsed_expr);
		#endif

			// temp buffer for building _SUBCODE and _REASON param names.
			std::string param_sub;

			std::string expr_string;
			param_sub = policy_name;
		#ifdef ENABLE_JOB_POLICY_LISTS
			policy.append_tag(param_sub);
		#endif
			param_sub += "_SUBCODE";
			if (param(expr_string, param_sub.c_str(), "") && ! expr_string.empty()) {
				long long ival;
				classad::Value val;
				if (ad.EvaluateExpr(expr_string, val) && val.IsNumber(ival)) {
					m_fire_subcode = (int)ival;
				}
			}

			param_sub = policy_name;
		#ifdef ENABLE_JOB_POLICY_LISTS
			policy.append_tag(param_sub);
		#endif
			param_sub += "_REASON";
			if (param(expr_string, param_sub.c_str(), "") && ! expr_string.empty()) {
				classad::Value val;
				if (ad.EvaluateExpr(expr_string, val) && val.IsStringValue(m_fire_reason)) {
					// val.IsStringValue will have already set m_fire_reason
				}
			}

			return true;
		}
	}

	return false;
}

const char* UserPolicy::FiringExpression(void)
{
	return m_fire_expr;
}

bool UserPolicy::FiringReason(std::string &reason,int &reason_code,int &reason_subcode)
{
	reason_code = 0;
	reason_subcode = 0;

	if ( m_fire_expr == NULL ) {
		return false;
	}

	reason = "";

	const char * expr_src = "UNKNOWN (never set)";
	std::string exprString;
	switch(m_fire_source) {
		case FS_NotYet:
			break;

		case FS_JobAttribute:
			expr_src = "job attribute";
			exprString = m_fire_unparsed_expr;
			if (m_fire_expr_val == -1) {
				reason_code = CONDOR_HOLD_CODE::JobPolicyUndefined;
			} else {
				reason_code = CONDOR_HOLD_CODE::JobPolicy;
				reason_subcode = m_fire_subcode;
				reason = m_fire_reason;
			}
			break;

		case FS_JobDuration:
			reason = m_fire_reason;
			reason_code = CONDOR_HOLD_CODE::JobDurationExceeded;
			reason_subcode = 0;
			break;

		case FS_ExecuteDuration:
			reason = m_fire_reason;
			reason_code = CONDOR_HOLD_CODE::JobExecuteExceeded;
			reason_subcode = 0;
			break;

		case FS_SystemMacro:
			expr_src = "system macro";
			exprString = m_fire_unparsed_expr;
			if( m_fire_expr_val == -1 ) {
				reason_code = CONDOR_HOLD_CODE::SystemPolicyUndefined;
			}
			else {
				reason_code = CONDOR_HOLD_CODE::SystemPolicy;
				reason_subcode = m_fire_subcode;
				reason = m_fire_reason;
			}
			break;

		default:
			expr_src = "UNKNOWN (bad value)";
			break;
	}

	if( !reason.empty() ) {
		return true;
	}

	// Format up the reason string
	formatstr( reason, "The %s %s expression '%s' evaluated to ",
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
