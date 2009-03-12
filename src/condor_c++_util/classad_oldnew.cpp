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
#include "stream.h"
#include "condor_classad.h"

using namespace std;

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"

classad::ClassAd *
getOldClassAd( Stream *sock )
{
	classad::ClassAd *ad = new classad::ClassAd( );
	if( !ad ) { 
		return NULL;
	}
	if( !getOldClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}

bool
getOldClassAd( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs;
	string					buffer;
	char					*tmp;
	classad::ClassAd		*upd=NULL;
	static char				*inputLine = new char[ 10240 ];


	ad.Clear( );
	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		tmp = NULL;
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

		buffer += string(inputLine) + ";";
		free( tmp );
	}
	buffer += "]";

		// get type info
	if (!sock->get(inputLine)||!ad.InsertAttr("MyType",(string)inputLine)) {
		return false;
	}
	if (!sock->get(inputLine)|| !ad.InsertAttr("TargetType",
											   (string)inputLine)) {
		return false;
	}

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	classad::ClassAd *tmpAd = new classad::ClassAd( );
	tmpAd->Update( *upd );
	classad::ClassAd *tmpAd2 = AddExplicitTargets( tmpAd );
	ad.Update( *tmpAd );
	delete tmpAd;
	delete tmpAd2;
	delete upd;

	return true;
}

bool
getOldClassAdNoTypes( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs = 0; // Initialization clears Coverity warning
	string					buffer;
	char					*tmp;
	classad::ClassAd		*upd=NULL;
	static char				*inputLine = new char[ 10240 ];


	ad.Clear( );
	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		tmp = NULL;
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

		buffer += string(inputLine) + ";";
		free( tmp );
	}
	buffer += "]";

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	ad.Update( *upd );
	delete upd;

	return true;
}



bool putOldClassAd ( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdUnParser	unp;
	string						buf;
	const classad::ExprTree		*expr;

	int numExprs=0;
	classad::ClassAdIterator itor(ad);
	while( !itor.IsAfterLast( ) ) {
		itor.CurrentAttribute( buf, expr );
		if( strcasecmp( "MyType", buf.c_str( ) ) != 0 && 
				strcasecmp( "TargetType", buf.c_str( ) ) != 0 ) {
			numExprs++;
		}
		itor.NextAttribute( buf, expr );
	}
	
	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
		
	for( itor.ToFirst();
		 !itor.IsAfterLast();
		 itor.NextAttribute(buf, expr) ) {
		itor.CurrentAttribute( buf, expr );
		if( strcasecmp( "MyType", buf.c_str( ) ) == 0 || 
				strcasecmp( "TargetType", buf.c_str( ) ) == 0 ) {
			continue;
		}
		buf += " = ";
		unp.Unparse( buf, expr );
		if (!sock->put(buf.c_str())) return false;
	}

	// Send the type
	if (!ad.EvaluateAttrString("MyType",buf)) {
		buf="(unknown type)";
	}
	if (!sock->put(buf.c_str())) {
		return false;
	}

	if (!ad.EvaluateAttrString("TargetType",buf)) {
		buf="(unknown type)";
	}
	if (!sock->put(buf.c_str())) {
		return false;
	}

	return true;
}

bool
putOldClassAdNoTypes ( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdUnParser	unp;
	string						buf;
	const classad::ExprTree		*expr;

	int numExprs=0;
	classad::ClassAdIterator itor(ad);

	while( !itor.IsAfterLast( ) ) {
		numExprs++;
	}	

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
		
	for( itor.ToFirst();
		 !itor.IsAfterLast();
		 itor.NextAttribute(buf, expr) ) {
		itor.CurrentAttribute( buf, expr );
		buf += " = ";
		unp.Unparse( buf, expr );
		if (!sock->put(buf.c_str())) return false;
	}

	return true;
}
