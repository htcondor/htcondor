/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

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
