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
#include "user_job_policy.h"

/* This code tests basic permutations of the user policy code and also
	for backwards compatibility to oldtyle job ads. */

void test_oldstyle_with_exit(void);
void test_oldstyle_with_no_exit(void);
void test_user_policy_periodic_hold_yes(void);
void test_user_policy_periodic_exit_yes(void);
void test_user_policy_periodic_hold_no(void);
void test_user_policy_periodic_exit_no(void);
void test_user_policy_on_exit_hold_yes(void);
void test_user_policy_on_exit_remove_yes(void);
void test_user_policy_on_exit_hold_no(void);
void test_user_policy_on_exit_remove_no(void);


int main(void)
{
	test_oldstyle_with_exit();
	test_oldstyle_with_no_exit();
	test_user_policy_periodic_hold_yes();
	test_user_policy_periodic_exit_yes();
	test_user_policy_periodic_hold_no();
	test_user_policy_periodic_exit_no();
	test_user_policy_on_exit_hold_yes();
	test_user_policy_on_exit_remove_yes();
	test_user_policy_on_exit_hold_no();
	test_user_policy_on_exit_remove_no();

	return 0;
}

void test_oldstyle_with_exit(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("==================================================\n");
	printf("Testing OldStyle job where it is marked as exited.\n");

	/* An oldstyle classad would have this */
	jad->Assign(ATTR_COMPLETION_DATE, 10); /* non zero */

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);
	}
	else
	{
		printf("Something went wrong. I should have had an action to take.\n");
	}
}


void test_oldstyle_with_no_exit(void)
{
	int val;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("======================================================\n");
	printf("Testing OldStyle job where it is NOT marked as exited.\n");

	/* An oldstyle classad would have this */
	jad->Assign(ATTR_COMPLETION_DATE, 0);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("Something went wrong. I should not have had an action.\n");
	}
	else
	{
		printf("Ignoring correctly.\n");
	}

	delete result;
}


void test_user_policy_periodic_hold_yes(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("=========================================\n");
	printf("Testing User Policy on Periodic Hold: YES\n");

	/* set up the classad */
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 42);
	jad->AssignExpr(ATTR_PERIODIC_HOLD_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);
	jad->Assign(ATTR_JOB_STATUS, RUNNING);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);
	}
	else
	{
		printf("Something went wrong. I should have had an action to take.\n");
	}
}

void test_user_policy_periodic_exit_yes(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("=========================================\n");
	printf("Testing User Policy on Periodic Exit: YES\n");

	/* set up the classad */
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 42);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->AssignExpr(ATTR_PERIODIC_REMOVE_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);
	jad->Assign(ATTR_JOB_STATUS, RUNNING);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);
	}
	else
	{
		printf("Something went wrong. I should have had an action to take.\n");
	}
}


void test_user_policy_periodic_hold_no(void)
{
	int val;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("========================================\n");
	printf("Testing User Policy on Periodic Hold: NO\n");

	/* set up the classad */
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 40);
	jad->AssignExpr(ATTR_PERIODIC_HOLD_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);
	jad->Assign(ATTR_JOB_STATUS, RUNNING);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("Something went wrong. I should not have had an action.\n");
	}
	else
	{
		printf("Ignoring correctly.\n");
	}
}


void test_user_policy_periodic_exit_no(void)
{
	int val;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("=========================================\n");
	printf("Testing User Policy on Periodic Exit: NO\n");

	/* set up the classad */
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 40);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->AssignExpr(ATTR_PERIODIC_REMOVE_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("Something went wrong. I should not have had an action.\n");
	}
	else
	{
		printf("Ignoring correctly.\n");
	}
}


void test_user_policy_on_exit_hold_yes(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("=========================================\n");
	printf("Testing User Policy on On Exit Hold: YES\n");

	/* set up the classad */
	jad->Assign(ATTR_ON_EXIT_CODE, 0);
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 42);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->AssignExpr(ATTR_ON_EXIT_HOLD_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);
	}
	else
	{
		printf("Something went wrong. I should have had an action to take.\n");
	}
}

void test_user_policy_on_exit_remove_yes(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("==========================================\n");
	printf("Testing User Policy on On Exit Remove: YES\n");

	/* set up the classad */
	jad->Assign(ATTR_ON_EXIT_CODE, 0);
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 42);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->AssignExpr(ATTR_ON_EXIT_REMOVE_CHECK, "TotalSuspensions == 42");

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);
	}
	else
	{
		printf("Something went wrong. I should have had an action to take.\n");
	}
}

void test_user_policy_on_exit_hold_no(void)
{
	int val;
	int action;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("=======================================\n");
	printf("Testing User Policy on On Exit Hold: NO\n");

	/* set up the classad */
	jad->Assign(ATTR_ON_EXIT_CODE, 0);
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 40);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->AssignExpr(ATTR_ON_EXIT_HOLD_CHECK, "TotalSuspensions == 42");
	jad->Assign(ATTR_ON_EXIT_REMOVE_CHECK, true);

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("%s was true.\n", ATTR_TAKE_ACTION);
		result->LookupInteger(ATTR_USER_POLICY_ACTION, action);
		printf("Action is: %s\n", 
			action==REMOVE_JOB?"REMOVE_JOB":action==HOLD_JOB?"HOLD_JOB":
			"UNKNOWN");
		result->LookupString(ATTR_USER_POLICY_FIRING_EXPR, buf);
		printf("Reason for action: %s\n", buf);

		if (strcmp(ATTR_USER_POLICY_FIRING_EXPR, ATTR_ON_EXIT_HOLD_CHECK) == 0)
		{
			printf("Failed. I got removed because exit_hold was true.\n");
		}
		else
		{
			printf("Success. I was removed because of %s, not because of %s\n", 
				ATTR_ON_EXIT_REMOVE_CHECK, ATTR_ON_EXIT_HOLD_CHECK);
		}
	}
	else
	{
		printf("Ignoring correctly.\n");
	}
}

void test_user_policy_on_exit_remove_no(void)
{
	int val;
	char buf[4096];
	ClassAd *result;
	ClassAd *jad = new ClassAd;
	if (jad == NULL)
	{
		printf("Out of memory!\n");
		exit(EXIT_FAILURE);
	}
	
	printf("========================================\n");
	printf("Testing User Policy on On Exit Remove: NO\n");

	/* set up the classad */
	jad->Assign(ATTR_ON_EXIT_CODE, 0);
	jad->Assign(ATTR_TOTAL_SUSPENSIONS, 40);
	jad->Assign(ATTR_PERIODIC_HOLD_CHECK, false);
	jad->Assign(ATTR_PERIODIC_REMOVE_CHECK, false);
	jad->Assign(ATTR_ON_EXIT_HOLD_CHECK, false);
	jad->AssignExpr(ATTR_ON_EXIT_REMOVE_CHECK, "TotalSuspensions == 42");

	result = user_job_policy(jad);

	result->EvalBool(ATTR_USER_POLICY_ERROR, result, val);
	if(val == true)
	{
		printf("An error happened\n");
		delete result;
		return;
	}

	result->EvalBool(ATTR_TAKE_ACTION, result, val);
	if (val == true)
	{
		printf("Something went wrong. I should not have had an action.\n");
	}
	else
	{
		printf("Ignoring correctly.\n");
	}
}


