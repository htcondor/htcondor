#ifndef USER_JOB_POLICY_H
#define USER_JOB_POLICY_H

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"

/* determine what to do with this job. */
ClassAd* user_job_policy(ClassAd *jad);

/* print out this expression nicely */
void EmitExpression(unsigned int mode, const char *attr, ExprTree* attr_expr);

/* Errors that can happen when determining the user_policy */
#define USER_ERROR_NOT_JOB_AD 0
#define USER_ERROR_INCONSISTANT 1

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
