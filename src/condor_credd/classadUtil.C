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

#include "classadUtil.h"

using namespace std;

BEGIN_NAMESPACE( classad )


bool getOldClassAd( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 			numExprs;
	string			buffer;
	char			*tmp;
	classad::ClassAd			*upd=NULL;
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

#if 0
		// put exprs into ad
	classad::ClassAd *tmpAd = new classad::ClassAd( );
	tmpAd->Update( *upd );
	tmpAd = tmpAd->AddExplicitTargetRefs( );
#endif
	ad.Update( *upd );
#if 0
	delete tmpAd;
#endif
	delete upd;

	return true;
}

bool classad::putOldClassAd ( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdUnParser	unp;
	string			buf;
	classad::ExprTree		*expr;

	int numExprs=0;
	classad::ClassAdIterator itor(ad);
	while( !itor.IsAfterLast( ) ) {
		itor.CurrentAttribute( buf, (const classad::ExprTree *)expr );
		if( strcasecmp( "MyType", buf.c_str( ) ) != 0 && 
				strcasecmp( "TargetType", buf.c_str( ) ) != 0 ) {
			numExprs++;
		}
		itor.NextAttribute( buf, (const classad::ExprTree *) expr );
	}
	
	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
		
	for( itor.ToFirst(); !itor.IsAfterLast(); itor.NextAttribute(buf, (const
					classad::ExprTree *) expr) ) {
		itor.CurrentAttribute( buf, (const classad::ExprTree *) expr );
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
