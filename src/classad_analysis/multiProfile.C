/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2005, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "multiProfile.h"

MultiProfile::
MultiProfile( )
{
}

MultiProfile::
~MultiProfile( )
{
}

bool MultiProfile::
AddProfile( Profile &c )
{
	return true;
}

bool MultiProfile::
RemoveProfile( Profile &c )
{
	return true;
}

bool MultiProfile::
RemoveAllProfiles( )
{
	return true;
}

bool MultiProfile::
IsLiteral( )
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


