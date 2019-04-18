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


#include "classad/classad_distribution.h"
#include "classad/fnCall.h"
#include <time.h>

/***************************************************************************
 *
 * This is a sample shared library that shows how to write your own
 * user-defined functions within a shared library. By the way, it also
 * shows how to write user-defined functions that aren't in a shared
 * library. They are registered with FunctionCall::RegisterFunction() instead
 * of through the Init function that a shared library has.
 *
 ***************************************************************************/

using namespace std;
using namespace classad;

static bool todays_date(const char *name, const ArgumentList &arguments,
    EvalState &state, Value  &result);
static bool doublenum(const char *name, const ArgumentList &arguments,
    EvalState &state, Value  &result);

/***************************************************************************
 *
 * This is the table that maps function names to functions. This is desirable
 * because we don't want all possible functions in your library to be
 * callable from the ClassAd code, but only the ones you want to expose.
 * Also, you can name the functions anything you want. Note that you can
 * list a function twice--it can check to see what it was called when 
 * it was invoked (like a program's argv[0] and change it's behavior. 
 * This can be useful when you have functions that are nearly identical.
 * There are some examples in fnCall.C that do this, like sumAvg and minMax.
 *
 ***************************************************************************/
static ClassAdFunctionMapping functions[] = 
{
    { "todays_date", (void *) todays_date, 0 },
	{ "double",      (void *) doublenum,   0 },
	{ "triple",      (void *) doublenum,   0 },
    { "",            NULL,                 0 }
};

/***************************************************************************
 * 
 * Function: Init
 * Purpose:  This is the entry point for this library. It must be 
 *           declared extern "C". All it does is return a pointer to 
 *           the table of functions we declared above. It could also
 *           do some additional set up, should you wish. 
 *
 ***************************************************************************/
extern "C" 
{

	ClassAdFunctionMapping *Init(void)
	{
		return functions;
	}
}

/****************************************************************************
 *
 * Function: todays_date
 * Purpose:  This is a ClassAd function, and it is called during the
 *           evaluation of a ClassAd when a user uses the todays_date 
 *           function. (See the name mapping above, in the functions list.) 
 *           It returns a string that shows today's date.
 * Returns:  It returns true if the function can be evaluated successfully,
 *           and false otherwise.
 *
 ****************************************************************************/
static bool todays_date(
	const char         *, // name
	const ArgumentList &arguments,
	EvalState          &, // state
	Value              &result)
{
	bool    eval_successful;
	
	// We check to make sure that we are passed exactly zero arguments,
	if (arguments.size() != 0) {
		result.SetErrorValue();
		eval_successful = false;
	} else {
		time_t    current_seconds;
		struct tm *current_time;
		char      time_string[100];

		// Create the result we want. 
		current_seconds = time(NULL);
		current_time = localtime(&current_seconds);
		snprintf(time_string, 100, "%d-%02d-%02d", current_time->tm_year+1900,
				 current_time->tm_mon+1, current_time->tm_mday);

		// Copy the result into the "result" variable.
		result.SetStringValue(time_string);
		eval_successful = true;
	}

	return eval_successful;
}

/****************************************************************************
 *
 * Function: doublenum
 * Purpose:  This is a ClassAd function, and it is called during the
 *           evaluation of a ClassAd when a user uses the double function. 
 *           (See the name mapping above, in the functions list.) 
 *           It just doubles a number.
 * Returns:  It returns true if the function can be evaluated successfully,
 *           and false otherwise.
 *
 ****************************************************************************/
static bool doublenum(
	const char         *, // name
	const ArgumentList &arguments,
	EvalState          &state,
	Value              &result)
{
	Value	arg;
	bool    eval_successful;
	
	// We check to make sure that we are passed exactly one argument,
	// then we have to evaluate that argument.
	if (arguments.size() != 1) {
		result.SetErrorValue();
		eval_successful = false;
	} else 	if (!arguments[0]->Evaluate(state, arg)) {
		result.SetErrorValue();
		eval_successful = false;
	} else {
		// Here we check for the type of the argument and do
		// the appropriate thing. 
		// If it was undefined, we should "pass up" the undefined. 
		// If it was an error, or a type we can't deal with, it's an error
		// For numbers, we do the right thing, and we're careful
		// about using either integers or reals. 
		switch (arg.GetType()) {
		case Value::UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			eval_successful = true;
			break;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::SCLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::SLIST_VALUE:
		case Value::STRING_VALUE:
		case Value::ABSOLUTE_TIME_VALUE:
		case Value::RELATIVE_TIME_VALUE:
		case Value::BOOLEAN_VALUE:
			result.SetErrorValue( );
			eval_successful = true;
			break;

		case Value::INTEGER_VALUE:
			int int_value;

			arg.IsIntegerValue(int_value);
			result.SetIntegerValue(2 * int_value);
			eval_successful = true;
			break;

		case Value::REAL_VALUE:
			double real_value;
			arg.IsRealValue( real_value );
			result.SetRealValue(2 * real_value);
			eval_successful = true;
			break;
		
		default:
			eval_successful = false;
			break;
		}
	}

	return eval_successful;
}
