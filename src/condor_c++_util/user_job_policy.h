#ifndef USER_JOB_POLICY_H
#define USER_JOB_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"

/* This code figures out what to do with a job given the user policy
denotated in the submit description file(and therefore in the job
ad). Here are the steps necessary to use this code:

1. If you are doing periodic calls to this function, then just check the
result classad and see what you need to do with the job. This is done like 
this:
	a) check to see if there is an error condition, then
	b) check if there needs to be an action taken.

2. If you are checking the job ad after the job has exited, then make sure you
	fix up the job ad with the exit/signal code BEFORE you call the user policy
	function.

3. If the job has exited for real, but the user_policy code(for whatever
reason) says it shouldn't leave the queue, then make sure you UNDEFINE
ATTR_ON_EXIT_CODE, ATTR_ON_EXIT_SIGNAL, and set ATTR_EXIT_BY_SIGNAL
to FALSE. This undoes the exiting of the job with repect to the
classad. Also, you have to set ATTR_COMPLETION_DATE back to zero.

Now, what do you do with the result classad?

A. check to see if ATTR_USER_POLICY_ERROR is true, if so, then the
attribute ATTR_USER_ERROR_REASON will contain a reason for the error.

B. check to see ATTR_TAKE_ACTION is true, if so, then
ATTR_USER_POLICY_FIRING_EXPR will contain the offending expression or the
text in "old_style_exit" that explains which expression caused the action.
The attribute ATTR_USER_POLICY_ACTION dictates what you should DO with
the job, either put it on hold or remove it from the queue.

C. If ATTR_USER_POLICY_FIRING_EXPR is ATTR_PERIODIC_REMOVE_CHECK then
"condor" removed the job and it should be treated differently than if
the job exited legitimately. For example, a different email to the user
explaining the job was forcibly removed because the periodic remove check
became true, not because the job exited legitimately.

D. The general case of no action specified means the job stays in the queue.

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

#endif
