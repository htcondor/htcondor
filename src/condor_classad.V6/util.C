/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"

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
	ad.Update( *upd );
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

	unp.SetOldClassAd(true);  // NAC

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


END_NAMESPACE // classad
