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
	bool periodic_hold = 0, periodic_remove = 0;
	bool on_exit_hold = 0, on_exit_remove = 0;
	int cdate = 0;
	int adkind;
	Value v;
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
    v.SetBooleanValue(false);
	result->Insert( string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v));
    v.Clear();
    v.SetBooleanValue(false);
	result->Insert( string(ATTR_USER_POLICY_ERROR), Literal::MakeLiteral(v));

	/* figure out the ad kind and then do something with it */

	adkind = JadKind(jad);

	switch(adkind)
	{
		case USER_ERROR_NOT_JOB_AD:
			dprintf(D_ALWAYS, "user_job_policy(): I have something that "
					"doesn't appear to be a job ad! Ignoring.\n");
            v.Clear();
            v.SetBooleanValue(true);
			result->Insert(string(ATTR_USER_POLICY_ERROR), Literal::MakeLiteral(v));
            v.Clear();
            v.SetIntegerValue((int)USER_ERROR_NOT_JOB_AD);
			result->Insert( string(ATTR_USER_ERROR_REASON),Literal::MakeLiteral(v) );

			return result;
			break;

		case USER_ERROR_INCONSISTANT:
			dprintf(D_ALWAYS, "user_job_policy(): Inconsistant jobad state "
								"with respect to user_policy. Detail "
								"follows:\n");
			{
				ExprTree *ph_expr = jad->Lookup(ATTR_PERIODIC_HOLD_CHECK);
				ExprTree *pr_expr = jad->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
				ExprTree *oeh_expr = jad->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
				ExprTree *oer_expr = jad->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

				EmitExpression(D_ALWAYS, ATTR_PERIODIC_HOLD_CHECK, ph_expr);
				EmitExpression(D_ALWAYS, ATTR_PERIODIC_REMOVE_CHECK, pr_expr);
				EmitExpression(D_ALWAYS, ATTR_ON_EXIT_HOLD_CHECK, oeh_expr);
				EmitExpression(D_ALWAYS, ATTR_ON_EXIT_REMOVE_CHECK, oer_expr);
			}

            v.Clear();
            v.SetBooleanValue(true);
			result->Insert( string(ATTR_USER_POLICY_ERROR),Literal::MakeLiteral(v) );
            v.Clear();
            v.SetIntegerValue((int)USER_ERROR_INCONSISTANT);
			result->Insert(string(ATTR_USER_ERROR_REASON), Literal::MakeLiteral(v) );

			return result;
			break;

		case KIND_OLDSTYLE:
			if (jad->EvaluateAttrInt(ATTR_COMPLETION_DATE, cdate)) {
                if (cdate > 0)
                    {
                        v.Clear();
                        v.SetBooleanValue(true);
                        result->Insert( string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v) );
                        v.Clear();
                        v.SetIntegerValue((int) REMOVE_JOB);
                        result->Insert(string(ATTR_USER_POLICY_ACTION), Literal::MakeLiteral(v));

                        v.Clear();
                        sprintf(buf, "\"%s\"", old_style_exit);
                        v.SetStringValue(buf);
                        result->Insert(string(ATTR_USER_POLICY_FIRING_EXPR),Literal::MakeLiteral(v));
                    }
                return result;
            }
            else {
                return NULL;
            }
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
			jad->EvaluateAttrBool( string(ATTR_PERIODIC_HOLD_CHECK), periodic_hold);
			if (periodic_hold)
			{
				/* make a result classad explaining this and return it */

                v.Clear();
                v.SetBooleanValue(true);
				result->Insert( string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v));
                v.Clear();
                v.SetIntegerValue((int) HOLD_JOB);
				result->Insert( string(ATTR_USER_POLICY_ACTION), Literal::MakeLiteral(v));
                v.Clear();
                // This is probably not necessary since we are using string literals
				sprintf(buf, "\"%s\"", ATTR_PERIODIC_HOLD_CHECK);
                v.SetStringValue(buf);
				result->Insert( string(ATTR_USER_POLICY_FIRING_EXPR), Literal::MakeLiteral(v) );

				return result;
			}

			/* Should I perform a periodic remove? */
			jad->EvaluateAttrBool( string(ATTR_PERIODIC_REMOVE_CHECK), periodic_remove);
			if (periodic_remove)
			{
				/* make a result classad explaining this and return it */

                v.Clear();
                v.SetBooleanValue(true);
				result->Insert(string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                v.SetIntegerValue((int) REMOVE_JOB);
				result->Insert( string(ATTR_USER_POLICY_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                sprintf(buf, "\"%s\"", ATTR_PERIODIC_REMOVE_CHECK);
                v.SetStringValue(buf);
				result->Insert( string(ATTR_USER_POLICY_FIRING_EXPR),Literal::MakeLiteral(v) );

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
			jad->EvaluateAttrBool(ATTR_ON_EXIT_HOLD_CHECK, on_exit_hold);
			if (on_exit_hold)
			{
				/* make a result classad explaining this and return it */

                v.Clear();
                v.SetBooleanValue(true);
				result->Insert(string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                v.SetIntegerValue((int) HOLD_JOB);
				result->Insert( string(ATTR_USER_POLICY_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                sprintf(buf, "\"%s\"", ATTR_PERIODIC_REMOVE_CHECK);
                v.SetStringValue(buf);
				result->Insert( string(ATTR_USER_POLICY_FIRING_EXPR),Literal::MakeLiteral(v) );

				return result;
			}

			/* Should I remove on exit? */
			jad->EvaluateAttrBool(ATTR_ON_EXIT_REMOVE_CHECK, on_exit_remove);
			if (on_exit_remove)
			{
				/* make a result classad explaining this and return it */

                v.Clear();
                v.SetBooleanValue(true);
				result->Insert(string(ATTR_TAKE_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                v.SetIntegerValue((int) REMOVE_JOB);
				result->Insert( string(ATTR_USER_POLICY_ACTION), Literal::MakeLiteral(v));

                v.Clear();
                sprintf(buf, "\"%s\"", ATTR_ON_EXIT_REMOVE_CHECK);
                v.SetStringValue(buf);
				result->Insert( string(ATTR_USER_POLICY_FIRING_EXPR),Literal::MakeLiteral(v) );

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
	//char buf[8192]; /* PrintToStr is dumb, hope this is big enough */

	if (attr_expr == NULL)
	{
		dprintf(mode, "%s = UNDEFINED\n", attr);
	}
	else
	{
        // Need work. Hao
		//attr_expr->PrintToStr(buf);
		//dprintf(mode, "%s = %s\n", attr, buf);
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
	/* determine if I have a user job ad with the new user policy expressions
		enabled. */
	ExprTree *ph_expr = suspect->Lookup(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = suspect->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *oeh_expr = suspect->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = suspect->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

	/* check to see if it is oldstyle */
	if (ph_expr == NULL && pr_expr == NULL && oeh_expr == NULL && 
		oer_expr == NULL)
	{
		/* check to see if it has ATTR_COMPLETION_DATE, if so then it is
			an oldstyle jobad. If not, it isn't a job ad at all. */

		if (!suspect->Lookup(ATTR_COMPLETION_DATE))
		{
			return KIND_OLDSTYLE;
		}
		return USER_ERROR_NOT_JOB_AD;
	}

	/* check to see if it is a consistant user policy job ad. */
	if (ph_expr == NULL || pr_expr == NULL || oeh_expr == NULL || 
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
    Value v;

	ExprTree *ph_expr = m_ad->Lookup(ATTR_PERIODIC_HOLD_CHECK);
	ExprTree *pr_expr = m_ad->Lookup(ATTR_PERIODIC_REMOVE_CHECK);
	ExprTree *oeh_expr = m_ad->Lookup(ATTR_ON_EXIT_HOLD_CHECK);
	ExprTree *oer_expr = m_ad->Lookup(ATTR_ON_EXIT_REMOVE_CHECK);

	/* if the default user policy expressions do not exist, then add them
		here and now with the usual defaults */
	if (ph_expr == NULL) {
        v.Clear();
        v.SetBooleanValue(false);
		m_ad->Insert( string(ATTR_PERIODIC_HOLD_CHECK), Literal::MakeLiteral(v));
	}

	if (pr_expr == NULL) {
        v.Clear();
        v.SetBooleanValue(false);
		m_ad->Insert( string(ATTR_PERIODIC_REMOVE_CHECK), Literal::MakeLiteral(v));
	}

	if (oeh_expr == NULL) {
        v.Clear();
        v.SetBooleanValue(false);
		m_ad->Insert( string(ATTR_ON_EXIT_HOLD_CHECK), Literal::MakeLiteral(v));
	}

	if (oer_expr == NULL) {
        v.Clear();
        v.SetBooleanValue(false);
        m_ad->Insert( string(ATTR_ON_EXIT_REMOVE_CHECK), Literal::MakeLiteral(v));
	}
}

int
UserPolicy::AnalyzePolicy( int mode )
{
	bool periodic_hold, periodic_remove;
	bool on_exit_hold, on_exit_remove;

	if (m_ad == NULL)
	{
		EXCEPT("UserPolicy Error: Must call Init() first!");
	}

	if (mode != PERIODIC_ONLY && mode != PERIODIC_THEN_EXIT)
	{
		EXCEPT("UserPolicy Error: Unknown mode in AnalyzePolicy()");
	}

		// Clear out our stateful variables
	m_fire_expr = NULL;
	m_fire_expr_val = -1;

	/*	The user_policy is checked in this
			order. The first one to succeed is the winner:
	
			ATTR_PERIODIC_HOLD_CHECK
			ATTR_PERIODIC_REMOVE_CHECK
			ATTR_ON_EXIT_HOLD_CHECK
			ATTR_ON_EXIT_REMOVE_CHECK
	*/

	/* should I perform a periodic hold? */
	m_fire_expr = ATTR_PERIODIC_HOLD_CHECK;
	if( ! m_ad->EvaluateAttrBool(ATTR_PERIODIC_HOLD_CHECK, periodic_hold) ) {
		return UNDEFINED_EVAL;
	}
	if( periodic_hold ) {
		m_fire_expr_val = 1;
		return HOLD_IN_QUEUE;
	}

	/* Should I perform a periodic remove? */
	m_fire_expr = ATTR_PERIODIC_REMOVE_CHECK;
	if(!m_ad->EvaluateAttrBool(ATTR_PERIODIC_REMOVE_CHECK, periodic_remove)) {
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
	if( ! m_ad->EvaluateAttrBool(ATTR_ON_EXIT_HOLD_CHECK, on_exit_hold) ) {
		return UNDEFINED_EVAL;
	}
	if( on_exit_hold ) {
		m_fire_expr_val = 1;
		return HOLD_IN_QUEUE;
	}

	/* Should I remove on exit? */
	m_fire_expr = ATTR_ON_EXIT_REMOVE_CHECK;
	if( ! m_ad->EvaluateAttrBool(ATTR_ON_EXIT_REMOVE_CHECK, on_exit_remove) ) {
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











