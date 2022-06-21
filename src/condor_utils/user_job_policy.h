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

#ifndef USER_JOB_POLICY_H
#define USER_JOB_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"

#define ENABLE_JOB_POLICY_LISTS 1
#ifdef ENABLE_JOB_POLICY_LISTS
  #include "compat_classad_util.h" // for ConstraintHolder
#else

/*
 * The user_job_policy() function is deprecated and NOT to be used for
 * new code. Pete Keller said so. :-)
 * Use the UserPolicy class, marked as the "NEW INTERFACE" below.
 *      -- jfrey, Nov 14, 2002
 */
/*
This is a plain english description of the technical details of the use
of the user_job_policy() function.

SYNOPSIS:

user_job_policy() takes a job classad and then determines if the
user specified policy in the job ad causes some sort of action to be
taken. This action could be to hold the job, to remove the job, or if
the job exited, ignore that fact and keep the job in the queue. This
action(and its properties) are represented as a classad pointer returned
from the function that you are responsible for freeing.

RETURN VALUE:

The resultant classad will always ATTR_USER_POLICY_ERROR defined as a
boolean. If ATTR_USER_POLICY_ERROR is true, then there was a problem
with the classad and ATTR_USER_ERROR_REASON will hold the reason.

If there wasn't an error with the classad, then inspect ATTR_TAKE_ACTION
and see if it is true or false. If false, then the job is supposed to
be left in the queue and you must reset the classad to this effect(if
the job had exited, there might be exit codes and what not in the
classad you need to set back into the undefined state).  If true,
then ATTR_USER_POLICY_ACTION explains what you should DO with the job,
either removing it or holding it. If you want to know why the action
happened, then you can inspect ATTR_USER_POLICY_FIRING_EXPR and this
will hold the offending attribute name of the expression that fired in
the user policy.  This last attribute allows for better output to the
user log about why something happened. Is the job going to be removed
intentionally by condor?  or did it just finish normally? Those are the
kinds of questions you could resolve when the user policy is invoked.

-psilord 10/18/2001
*/

/* determine what to do with this job. */
ClassAd* user_job_policy(ClassAd *jad);

/* determine what kind of classad it is with respect to the user_policy */
int JadKind(ClassAd *suspect);

/* print out this expression nicely */
void EmitExpression(unsigned int mode, const char *attr, ExprTree* attr_expr);

/* Errors that can happen when determining the user_policy */
#define USER_ERROR_NOT_JOB_AD 0
#define USER_ERROR_INCONSISTANT 1

/* the "kind" that a job ad candidate can be */
#define KIND_OLDSTYLE 2
#define KIND_NEWSTYLE 3 /* new style is with the new user policy stuff */

/* If a job ad was pre user policy and it was determined to have exited. */
extern const char *old_style_exit;

/* Job actions determined */
#define REMOVE_JOB 0
#define HOLD_JOB 1

/* This will be one of the job actions defined above */
extern const char ATTR_USER_POLICY_ACTION[];

/* This is one of: ATTR_PERIODIC_HOLD_CHECK, ATTR_PERIODIC_REMOVE_CHECK,
	ATTR_ON_EXIT_HOLD_CHECK, ATTR_ON_EXIT_REMOVE_CHECK, or
	old_style_exit. It allows killer output of what happened and why. And
	since it is defined in terms of other expressions, makes it easy
	to compare against. */
extern const char ATTR_USER_POLICY_FIRING_EXPR[];

/* true or false, true if it is determined the job should be held or removed
	from the queue. If false, then the caller should put this job back into
	the idle state and undefine these attributes in the job ad:
	ATTR_ON_EXIT_CODE, ATTR_ON_EXIT_SIGNAL, and then change this attribute
	ATTR_ON_EXIT_BY_SIGNAL to false in the job ad. */
extern const char ATTR_TAKE_ACTION[];

/* If there was an error in determining the policy, this will be true. */
extern const char ATTR_USER_POLICY_ERROR[];

/* an "errno" of sorts as to why the error happened. */
extern const char ATTR_USER_ERROR_REASON[];

#endif // ENABLE_JOB_POLICY_LISTS

/* NEW INTERFACE */

enum { STAYS_IN_QUEUE = 0, REMOVE_FROM_QUEUE, HOLD_IN_QUEUE, UNDEFINED_EVAL, RELEASE_FROM_HOLD };
enum { PERIODIC_ONLY = 0, PERIODIC_THEN_EXIT };

/* ok, here is the first set of expressions that should be available
	in the classad when it is given to Init():

	ATTR_TIMER_REMOVE_CHECK
	ATTR_PERIODIC_HOLD_CHECK
	ATTR_PERIODIC_RELEASE_CHECK
	ATTR_PERIODIC_REMOVE_CHECK
	ATTR_ON_EXIT_HOLD_CHECK
	ATTR_ON_EXIT_REMOVE_CHECK

	If any of the above attributes besides ATTR_TIMER_REMOVE_CHECK are 
	not present, then they will be assigned defaults and inserted into 
	the classad.
	The defaults are: False, False, False, False, True, respectively.

	Now, if you are using mode PERIODIC_ONLY in AnalyzePolicy(),
	then this is all that you need in the classad _plus_ any other
	attributes specified by the above expressions needed during
	their evaluation. If any of the above expressions evaluate to
	undefined, then the UserPolicy class will return UNDEFINED_EVAL
	in AnalyzePolicy() and the offending attribute can be seen with
	FiringExpression().

	If you are using PERIODIC_THEN_EXIT with AnalyzePolicy(),
	then you *also* need ATTR_ON_EXIT_BY_SIGNAL in the classad. If
	ATTR_ON_EXIT_BY_SIGNAL == True, then ATTR_ON_EXIT_SIGNAL
	must also be present in the classad and set to the signal the
	process had died by. If ATTR_ON_EXIT_BY_SIGNAL == False, then
	ATTR_ON_EXIT_CODE must also be present in the classad and set
	to the exit value of the job.

	If ATTR_ON_EXIT_BY_SIGNAL is not present, AnalyzePolicy() will
	EXCEPT. If ATTR_ON_EXIT_BY_SIGNAL is present, but ATTR_ON_EXIT_SIGNAL
	and ATTR_ON_EXIT_CODE is not, then AnalyzePolicy() will EXCEPT.

	If PERIODIC_ONLY is used with AnalyzePolicy(), then only 
	ATTR_TIMER_REMOVE_CHECK, ATTR_PERIODIC_HOLD_CHECK,
	ATTR_PERIODIC_REMOVE_CHECK and ATTR_PERIODIC_RELEASE_CHECK will be
	evaluated(in that order) to determine if anything should happen with
	the job.

	If PERIODIC_THEN_EXIT is used with AnalyzePolicy(), then
	ATTR_TIMER_REMOVE, ATTR_PERIODIC_HOLD_CHECK,
	ATTR_PERIODIC_REMOVE_CHECK, ATTR_PERIODIC_RELEASE_CHECK,
	ATTR_ON_EXIT_HOLD_CHECK, and ATTR_ON_EXIT_REMOVE_CHECK will be
	evaluated(in that order) to determine if anything should happen
	with the job.

	If AnalyzePolicy() says to do anything with the job, then
	you can use FiringExpression() to get a pointer to static
	memory (which is NOT freed by the caller) which details which
	expression caused the action. You can use strcmp against this
	string and the six attributes detailed at the top of the comment
	to figure out which expressions caused the action happened to
	the job.  You can also call FiringExpressionValue() to find out
	what the firing expression evaluated to which casued the action.

	If you do a periodic evaluation and none of the periodic check
	expressions became true, the action you get is STAYS_IN_QUEUE and
	the FiringExpression() will be NULL.  However, when you do an on
	exit evaluation, if you get STAYS_IN_QUEUE, that's because an
	expression fired (ON_EXIT_REMOVE_CHECK) and became false.  In this
	case, FiringExpression will return ON_EXIT_REMOVE_CHECK and
	FiringExpressionValue will be 0.
*/

#ifdef ENABLE_JOB_POLICY_LISTS

char * param(const char *); // forward ref from condor_config.h

// This class holds both a boolean expression
// and a Tag name for the expression
// for instance if the config has
//    SYSTEM_PERIODIC_HOLD_MEM = MemoryUsage > RequestMemory
// then the Tag will be MEM and the constraint will be MemoryUsage > RequestMemory
class JobPolicyExpr
{
public:
	JobPolicyExpr(const char * _name="") : name(_name) {}

	void set_from_config(const char * knob) { ch.set(param(knob)); }

	ExprTree * Expr(int * error=NULL) const { return ch.Expr(error); }
	const char * Str() const { return ch.Str(); }
	bool empty() const { return ch.empty(); }
	const char * Tag() const { return name.c_str(); }
	const char * append_tag(std::string & knob) const {
		if (!name.empty()) {
			knob += "_";
			knob += name;
		}
		return knob.c_str();
	}
	bool is_trivial(bool trivial_value=false) const {
		if (!empty()) {
			bool value = trivial_value;
			ExprTree * expr = Expr();
			if (expr && ExprTreeIsLiteralBool(Expr(), value) && value == trivial_value) {
				return true;
			}
		}
		return empty();
	}
protected:
	ConstraintHolder ch;
	std::string name;
};

#endif

class UserPolicy
{
	enum FireSource { FS_NotYet, FS_JobAttribute, FS_JobDuration, FS_ExecuteDuration, FS_SystemMacro };

	public: /* functions */
		UserPolicy()
			: m_fire_source(FS_NotYet)
			, m_fire_subcode(0)
			, m_fire_expr_val(-1)
			, m_fire_expr(nullptr)
		#ifdef ENABLE_JOB_POLICY_LISTS
		#else
			, m_sys_periodic_hold(NULL)
			, m_sys_periodic_release(NULL)
			, m_sys_periodic_remove(NULL)
		#endif
			{}

		~UserPolicy() {
			ClearConfig();
			m_fire_expr = NULL;
		}

		/* configure and reset triggers */
		void Init();
		/* clear the 'policy has fired' variables */
		void ResetTriggers();

		/* mode is PERIODIC_ONLY or PERIODIC_THEN_EXIT */
		/* state is the job status, looked up in the ad if not provided */
		/* returns STAYS_IN_QUEUE, REMOVE_FROM_QUEUE, HOLD_IN_QUEUE, 
			UNDEFINED_EVAL, or RELEASE_FROM_HOLD */
		int AnalyzePolicy(ClassAd &ad, int mode, int state = -1);

		/* This explains what expression caused the above action, if no 
			firing expression occurred, then return NULL. The user does NOT
			free this memory and it is overwritten whenever an Init() or
			AnalyzePolicy() method is called */
		const char* FiringExpression(void);

		/* This explains what the firing expression evaluated to which
		   caused the above action.  If no firing expression occured,
		   return -1. */
		int FiringExpressionValue( void ) const { return m_fire_expr_val; };

		/* This constructs the string explaining what expression fired, useful
		   for a Reason string in the job ad. If no firing expression
		   occurred, then false is returned. */
		bool FiringReason(std::string & reason, int & reason_code, int & reason_subcode);

	private: /* functions */
		/* This function inserts the five of the six (all but TimerRemove) user
			job policy expressions with default values into the classad if they
			are not already present. */
		void Config(void);
		void ClearConfig(void) {
		#ifdef ENABLE_JOB_POLICY_LISTS
				m_sys_periodic_holds.clear();
				m_sys_periodic_releases.clear();
				m_sys_periodic_removes.clear();
		#else
				delete m_sys_periodic_hold; m_sys_periodic_hold = NULL;
				delete m_sys_periodic_release; m_sys_periodic_release = NULL;
				delete m_sys_periodic_remove; m_sys_periodic_remove = NULL;
		#endif
			}

		/* I can't be copied */
		UserPolicy(const UserPolicy&);
		UserPolicy& operator=(const UserPolicy&);

		/*
		Consider a single periodic_* policy, both in the job ad and the
		system_peridoic_* version.  If true, we should return retval.  The
		retval should be on_true_return (if the expression evaluated to true),
		or UNDEFINED_EVAL if the classad is damaged.  If false, we didn't
		trigger the policy.  The details on what why will be automatically
		set.

		attrname - The job attribute to consider.  Should be one of the ATTR_*
		constants, as it may be stored beyond the lifespan of this function
		(but not the object).  The corresponding system version is found by
		simply stuffing "system_" in front of it.

		on_true_return - if the job attribute (or corresponding system one)
		evaluates to true, return this.

		retval - For output.  If this function returns true, this is the value
		that AnalyzePolicy should return.  If this function is false, retval is
		undefined.
		*/
		enum SysPolicyId { SYS_POLICY_NONE=0, SYS_POLICY_PERIODIC_HOLD, SYS_POLICY_PERIODIC_RELEASE, SYS_POLICY_PERIODIC_REMOVE };
		bool AnalyzeSinglePeriodicPolicy(ClassAd & ad, const char * attrname, SysPolicyId sys_policy, int on_true_return, int & retval);
		bool AnalyzeSinglePeriodicPolicy(ClassAd & ad, ExprTree * expr, int on_true_return, int & retval);

	private: /* variables */
		FireSource m_fire_source;
		int m_fire_subcode;
		int m_fire_expr_val;
		const char *m_fire_expr;
		std::string m_fire_reason;
		std::string m_fire_unparsed_expr;
	#ifdef ENABLE_JOB_POLICY_LISTS // multi policy
		std::vector<JobPolicyExpr> m_sys_periodic_holds;
		std::vector<JobPolicyExpr> m_sys_periodic_releases;
		std::vector<JobPolicyExpr> m_sys_periodic_removes;
	#else
		ExprTree * m_sys_periodic_hold;
		ExprTree * m_sys_periodic_release;
		ExprTree * m_sys_periodic_remove;
	#endif
};


#endif
