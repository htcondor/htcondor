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
#include "resourceGroup.h"

using namespace std;

ResourceGroup::
ResourceGroup( )
{
	initialized = false;
}

ResourceGroup::
~ResourceGroup( )
{
    classad::ClassAd *curr;
    classads.Rewind( );
	while( classads.Next( curr ) ) {
        delete curr;
	}
    return;
}

bool ResourceGroup::
Init( List<classad::ClassAd>& adList )
{
    classad::ClassAd *curr;
	adList.Rewind( );
	while( adList.Next( curr ) ) {
		if( !classads.Append( curr ) ) {
			return false;
		}
	}
	initialized = true;
	return true;
}

bool ResourceGroup::
GetClassAds( List<classad::ClassAd> &newList )
{
	if( !initialized ) {
		return false;
	}

	classad::ClassAd *curr;

	classads.Rewind( );
	while( classads.Next( curr ) ) {
		newList.Append( curr );
	}
	return true;
}

bool ResourceGroup::
GetNumberOfClassAds( int& num )
{
	if( !initialized ) {
		return false;
	}
	num = classads.Number( );
	return true;
}

bool ResourceGroup::
ToString( string& buffer )
{
	if( !initialized ) {
		return false;
	}
	classad::PrettyPrint pp;
	classad::ClassAd* curr;
	classads.Rewind( );
	while( classads.Next( curr ) ) {
		pp.Unparse( buffer, curr );
		buffer += "\n";
	}
	return true;
}
