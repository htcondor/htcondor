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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "condor_debug.h"
#include "newclassad_stream.unix.h"

using namespace std;

int
StreamPut( Stream *stream, const classad::ClassAd &ad )
{
	classad::ClassAdUnParser	unparser;
	string						str;
	unparser.Unparse( str, &ad );
	return stream->put( str.c_str() );
}

int
StreamPut( Stream *stream, list<const classad::ClassAd *> &ad_list )
{
	if ( !stream->put( (int) ad_list.size() ) ) {
		return 0;
	}
	list< const classad::ClassAd *>::iterator iter;
	for ( iter = ad_list.begin();  iter != ad_list.end();  iter++ )
	{
		const	classad::ClassAd	*ad = *iter;
		if ( !StreamPut( stream, *ad ) ) {
			return 0;
		}
	}
	return 1;
}

int StreamGet( Stream *stream, list<classad::ClassAd *> &ad_list )
{
	int		num_ads;
	if ( !stream->get( num_ads ) ) {
		return 0;
	}
	if ( num_ads < 0 ) {
		return 0;
	}
	for( int ad_num = 0;  ad_num < num_ads;  ad_num++ ) {
		classad::ClassAd	*ad = new classad::ClassAd;
		if ( !StreamGet( stream, *ad ) ) {
			return 0;
		}
		ad_list.push_back( ad );
	}
	return num_ads;
}

int StreamGet( Stream *stream, classad::ClassAd &ad )
{
	char		*cstr = NULL;
	if ( !stream->get( cstr ) ) {
		dprintf( D_FULLDEBUG, "get( %p ) failed\n", cstr );
		return 0;
	}
	classad::ClassAdParser	parser;
	if ( ! parser.ParseClassAd( cstr, ad, true ) ) {
		free( cstr );
		return 0;
	}
	free( cstr );
	return 1;
}
