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
/*
    This file implements the Reqexp class, which contains methods and
    data to manipulate the requirements expression of a given
    resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "startd.h"
static char *_FileName_ = __FILE__;

Reqexp::Reqexp( ClassAd** cap )
{
	this->cap = cap;
	char tmp[1024];
	sprintf( tmp, "%s = %s", ATTR_REQUIREMENTS, "START" );
	origreqexp = strdup( tmp );
	rstate = UNAVAIL;
}


Reqexp::~Reqexp()
{
	free( origreqexp );
}


int
Reqexp::eval() 
{
	ExprTree *tree;
	ClassAd ad;
	EvalResult res;

	Parse( origreqexp, tree );
	tree->EvalTree( *cap, &ad, &res);
	delete tree;

	if( res.type != LX_INTEGER ) {
		return -1;
	} else {
		return res.i;
	}
}


void
Reqexp::restore()
{
	if( rstate != ORIG ) {
		(*cap)->InsertOrUpdate( origreqexp );
		rstate = ORIG;
	}
}


void
Reqexp::unavail() 
{
	char tmp[100];
	sprintf( tmp, "%s = False", ATTR_REQUIREMENTS );
	(*cap)->InsertOrUpdate( tmp );
	rstate = UNAVAIL;
}


void
Reqexp::avail() 
{
	char tmp[100];
	sprintf( tmp, "%s = True", ATTR_REQUIREMENTS );
	(*cap)->InsertOrUpdate( tmp );
	rstate = AVAIL;
}


void
Reqexp::pub()
{
	switch( this->eval() ) {
	case 0:							// requirements == false
		this->unavail();
		break;
	case 1:							// requirements == true
		this->avail();		
		break;
	case -1:						// requirements undefined
		this->restore();			
		break;
	}
}


reqexp_state
Reqexp::update( ClassAd* ca )
{
	char tmp[100];
	switch( rstate ) {
	case ORIG:
		sprintf( tmp, "%s", origreqexp );
		break;
	case AVAIL:
		sprintf( tmp, "%s = True", ATTR_REQUIREMENTS );
		break;
	case UNAVAIL:
		sprintf( tmp, "%s = False", ATTR_REQUIREMENTS );
		break;
	}
	ca->InsertOrUpdate( tmp );
	return rstate;
}
