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

using namespace std;

BEGIN_NAMESPACE( classad )

ClassAd* getOldClassAd( Stream *sock )
{
	ClassAd *ad = new ClassAd( );
	if( !ad ) { 
		return (ClassAd*) 0;
	}
	if( !getOldClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}


bool getOldClassAd( Stream *sock, ClassAd& ad )
{
	ClassAdParser	parser;
	int 			numExprs;
	string			buffer;
	char			*tmp;
	ClassAd			*upd=NULL;
	static char		*inputLine = new char[ 10240 ];


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
	ClassAd *tmpAd = new ClassAd( );
	tmpAd->Update( *upd );
	tmpAd = tmpAd->AddExplicitTargetRefs( );
	ad.Update( *tmpAd );
	delete tmpAd;
	delete upd;

	return true;
}


bool getOldClassAdNoTypes( Stream *sock, ClassAd& ad )
{
	ClassAdParser	parser;
	int 			numExprs;
	string			buffer;
	char			*tmp;
	ClassAd			*upd=NULL;
	static char		*inputLine = new char[ 10240 ];


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



bool putOldClassAd ( Stream *sock, ClassAd& ad )
{
	ClassAdUnParser	unp;
	string			buf;
	ExprTree		*expr;

	int numExprs=0;
	ClassAdIterator itor(ad);
	while( !itor.IsAfterLast( ) ) {
		itor.CurrentAttribute( buf, (const ExprTree *)expr );
		if( strcasecmp( "MyType", buf.c_str( ) ) != 0 && 
				strcasecmp( "TargetType", buf.c_str( ) ) != 0 ) {
			numExprs++;
		}
		itor.NextAttribute( buf, (const ExprTree *) expr );
	}
	
	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
		
	for( itor.ToFirst(); !itor.IsAfterLast(); itor.NextAttribute(buf, (const ExprTree *) expr) ) {
		itor.CurrentAttribute( buf, (const ExprTree *) expr );
		if( strcasecmp( "MyType", buf.c_str( ) ) == 0 || 
				strcasecmp( "TargetType", buf.c_str( ) ) == 0 ) {
			continue;
		}
		buf += " = ";
		unp.Unparse( buf, expr );
		if (!sock->put((char*)buf.c_str())) return false;
	}

	// Send the type
	if (!ad.EvaluateAttrString("MyType",buf)) {
		buf="(unknown type)";
	}
	if (!sock->put((char*)buf.c_str())) {
		return false;
	}

	if (!ad.EvaluateAttrString("TargetType",buf)) {
		buf="(unknown type)";
	}
	if (!sock->put((char*)buf.c_str())) {
		return false;
	}

	return true;
}

bool putOldClassAdNoTypes ( Stream *sock, ClassAd& ad )
{
	ClassAdUnParser	unp;
	string			buf;
	ExprTree		*expr;

	int numExprs=0;
	ClassAdIterator itor(ad);

	while( !itor.IsAfterLast( ) ) {
		numExprs++;
	}	

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
		
	for( itor.ToFirst(); !itor.IsAfterLast(); itor.NextAttribute(buf, (const ExprTree *) expr) ) {
		itor.CurrentAttribute( buf, (const ExprTree *) expr );
		buf += " = ";
		unp.Unparse( buf, expr );
		if (!sock->put((char*)buf.c_str())) return false;
	}

	return true;
}

END_NAMESPACE // classad
