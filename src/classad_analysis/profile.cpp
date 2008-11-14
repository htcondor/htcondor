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
#include "profile.h"

Profile::
Profile( )
{
}

Profile::
~Profile( )
{
}

bool Profile::
AddCondition( Condition &c )
{
	return true;
}

bool Profile::
RemoveCondition( Condition &c )
{
	return true;
}

bool Profile::
RemoveAllConditions( )
{
	return true;
}

bool Profile::
GetNumberOfConditions( int &result )
{
	if( !initialized ) {
		return false;
	}
	result = conditions.Number( );
	return true;
}

bool Profile::
Rewind( )
{
	if( !initialized ) {
		return false;
	}
	conditions.Rewind( );
	return true;
}

bool Profile::
NextCondition( Condition *&result )
{
   	if( !initialized ) {
		return false;
	}
	return conditions.Next( result );
}

bool Profile::
AppendCondition( Condition * condition )
{
	if( !initialized ) {
		return false;
	}
	if( !condition ) {
		return false;
	}
	return conditions.Append( condition );
}
