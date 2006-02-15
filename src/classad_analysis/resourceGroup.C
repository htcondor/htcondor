/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
    int i = 0;
    classad::ClassAd *curr;
    classads.Rewind( );
	while( classads.Next( curr ) ) {
        delete curr;
        i++;
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
