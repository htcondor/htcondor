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
	int periodic_hold = 0, periodic_remove = 0;
	int on_exit_hold = 0, on_exit_remove = 0;
	int cdate = 0;
	
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

	/* in the versions of Condor that allow user defined policies to exist
		condor_submit inserts ATTR_PERIODIC_HOLD_CHECK, 
		ATTR_PERIODIC_REMOVE_CHECK, ATTR_ON_EXIT_HOLD_CHECK, and 
		ATTR_ON_EXIT_REMOVE_CHECK as default values. If these do not 
		exist, then I cannot check any periodic expressions, and must use
		a different scheme to figure out if the job exited. */

	ExprTree *ph_expr = jad->Lookup(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = jad->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *oeh_expr = jad->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = jad->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

	/* This determines if the user_policy expression definition appears in
		this classad. If not, then do old-style policy where I only check to
		see if the job exited correctly. */
		
	if (ph_expr == NULL && pr_expr == NULL && oeh_expr == NULL && 
		oer_expr == NULL)
	{
		/* do old style policy checking, only determine if job has exited. */

		if (jad->LookupInteger(ATTR_COMPLETION_DATE, cdate) == 0)
		{
			/* What? No Completion date and no user policy? I must not have
				a job ad or something like that. */
			dprintf(D_ALWAYS, "user_job_policy(): I have something that "
					"doesn't appear to be a job ad! Ingoring.\n");

			sprintf(buf, "%s = TRUE", ATTR_USER_POLICY_ERROR);
			result->Insert(buf);
			sprintf(buf, "%s = %u", ATTR_USER_ERROR_REASON, 
				USER_ERROR_NOT_JOB_AD);
			result->Insert(buf);

			return result;
		}

		if (cdate > 0)
		{
			/* oldstyle job ad said this job is finished */

			sprintf(buf, "%s = TRUE", ATTR_TAKE_ACTION);
			result->Insert(buf);
			sprintf(buf, "%s = %d", ATTR_USER_POLICY_ACTION, REMOVE_JOB);
			result->Insert(buf);
			sprintf(buf, "%s = \"%s\"", ATTR_USER_POLICY_FIRING_EXPR, 
				old_style_exit);
			result->Insert(buf);
		}

		return result;
	}

	/* I've determined that user_policy expressions in the job ad exist, so
		now see if they exist in a consistant way. */

	if (ph_expr == NULL || pr_expr == NULL || oeh_expr == NULL || 
		oer_expr == NULL)
	{
		dprintf(D_ALWAYS, "user_job_policy(): Inconsistant jobad state with "
							"respect to user_policy. Detail follows:\n");

		EmitExpression(D_ALWAYS, ATTR_PERIODIC_HOLD_CHECK, ph_expr);
		EmitExpression(D_ALWAYS, ATTR_PERIODIC_REMOVE_CHECK, pr_expr);
		EmitExpression(D_ALWAYS, ATTR_ON_EXIT_HOLD_CHECK, oeh_expr);
		EmitExpression(D_ALWAYS, ATTR_ON_EXIT_REMOVE_CHECK, oer_expr);

		sprintf(buf, "%s = TRUE", ATTR_USER_POLICY_ERROR);
		result->Insert(buf);
		sprintf(buf, "%s = %u", ATTR_USER_ERROR_REASON, 
			USER_ERROR_INCONSISTANT);
		result->Insert(buf);

		return result;
	}

	/* ok, now that I know the job ad is consistant, I can do
		the actual policy. The user_policy is checked in this
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

	/* Check to see if ExitSignal or ExitCode are defined, if not, then
		assume the job hadn't exited and don't check the policy. This could
		hide a mistake of the caller to insert those attributes correctly
		but allows checking of the job ad in a periodic context. */
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

	/* just return the default of do nothing */
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


