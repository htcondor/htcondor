#ifndef USER_JOB_POLICY_H
#define USER_JOB_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"

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

#endif
