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
#include "multiProfile.h"

#include <iostream>
using namespace std;

MultiProfile::
MultiProfile( )
{
	isLiteral = false;
	literalValue = UNDEFINED_VALUE;
}

MultiProfile::
~MultiProfile( )
{
	Profile *p;
	profiles.Rewind();
	while (profiles.Next(p)) {
		delete p;
	}
}

bool MultiProfile::
AddProfile( Profile & )
{
	return true;
}

bool MultiProfile::
RemoveProfile( Profile & )
{
	return true;
}

bool MultiProfile::
RemoveAllProfiles( )
{
	return true;
}

bool MultiProfile::
IsLiteral( ) const
{
	return isLiteral;
}

bool MultiProfile::
GetLiteralValue( BoolValue &result )
{
	if( !isLiteral ) {
		return false;
	}
	result = literalValue;
	return true;
}


bool MultiProfile::
GetNumberOfProfiles( int &result )
{
	if( !initialized ) {
		return false;
	}
	result = profiles.Number( );
	return true;
}

bool MultiProfile::
Rewind( )
{
	if( !initialized ) {
		return false;
	}
	profiles.Rewind( );
	return true;
}

bool MultiProfile::
NextProfile( Profile *&result )
{
   	if( !initialized ) {
		return false;
	}
	return profiles.Next( result );
}

bool MultiProfile::
ToString( string& buffer )
{
	if( !initialized ) {
		return false;
	}
	if( isLiteral ) {
		char valChar = '!';
		GetChar( literalValue, valChar );
		buffer += valChar;
	}
	else {
		classad::PrettyPrint pp;
		pp.Unparse( buffer, myTree );
	}
	return true;
}

bool MultiProfile::
AppendProfile( Profile * profile )
{
	if( !initialized ) {
		return false;
	}
	if( !profile ) {
		return false;
	}
	return profiles.Append( profile );
}

bool MultiProfile::
InitVal( classad::Value &val )
{
	bool bval;

	isLiteral = true;

	if( val.IsBooleanValue( bval ) ) {
		if( bval ) {
			literalValue = TRUE_VALUE;
		} else {
			literalValue = FALSE_VALUE;
		}
	} else if( val.IsUndefinedValue( ) ) {
		literalValue = UNDEFINED_VALUE;
	} else if( val.IsErrorValue( ) ) {
		literalValue = ERROR_VALUE;
	} else {
		cerr << "error: value not boolean, error, or undef" << endl;
		return false;
	}

	myTree = NULL;
	initialized = true;
	return true;
}


