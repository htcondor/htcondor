/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2002, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

// This is a program that does a bunch of testing that is easier to do
// without user input, like in test_classads.

#include "classad_distribution.h"
#include "fnCall.h"

using namespace std;
#ifdef WANT_NAMESPACES
using namespace classad;
#endif

bool doublenum(const char *name, const ArgumentList &arguments,
				   EvalState &state, Value  &result);

static ClassAdFunctionMapping functions[] = 
{
	{ "double", (void *) doublenum, 0 },
	{ "triple", (void *) doublenum, 0 },
    { "",       NULL, 0 }
};

//ClassAdSharedLibraryMapping functions[] = 
//{
//	{ "double", "doublenum", 0 },
//	{ "triple", "doublenum", 0 },
//   { NULL,      NULL,       0 }
//};

extern "C" 
{

	ClassAdFunctionMapping *Init(void)
	{
		return functions;
	}
}

bool doublenum(
	const char         *name,
	const ArgumentList &arguments,
	EvalState          &state,
	Value              &result)
{
	Value	arg;
	
	// takes exactly one argument
	if( arguments.size() > 1 ) {
		result.SetErrorValue( );
		return true;
	}
	if( !arguments[0]->Evaluate( state, arg ) ) {
		result.SetErrorValue( );
		return false;
	}

	switch( arg.GetType( ) ) {
		case Value::UNDEFINED_VALUE:
			result.SetUndefinedValue( );
			return true;

		case Value::ERROR_VALUE:
		case Value::CLASSAD_VALUE:
		case Value::LIST_VALUE:
		case Value::STRING_VALUE:
		case Value::ABSOLUTE_TIME_VALUE:
		case Value::RELATIVE_TIME_VALUE:
		case Value::BOOLEAN_VALUE:
			result.SetErrorValue( );
			return true;

		case Value::INTEGER_VALUE:
			int int_value;

			arg.IsIntegerValue(int_value);
			result.SetIntegerValue(2 * int_value);
			return true;

		case Value::REAL_VALUE:
			{
				double real_value;
				arg.IsRealValue( real_value );
				result.SetRealValue(2 * real_value);
				return true;
			}
		
		default:
			return false;
	}

	return false;
}
