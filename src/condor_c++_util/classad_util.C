/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2002 CONDOR Team, Computer Sciences Department, 
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
  This file holds utility functions that rely on ClassAds.
*/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "sig_name.h"

extern "C" {

/*
  This method is static to this file and shouldn't be used directly.
  it just does the actual work for findSoftKillSig() and
  findRmKillSig().  In here, we lookup the given attribute name, which
  specifies a signal we want to use.  The signal could either be
  stored as an int, or as a string.  if it's a string, we want to
  translate it into the appropriate signal number for this platform. 
*/
static int
findSignal( ClassAd* ad, const char* attr_name )
{
	if( ! ad ) {
		return -1;
	}
	char* name = NULL;

	ExprTree *tree, *rhs;
	tree = ad->Lookup( attr_name );
	if(  tree ) {
		rhs = tree->RArg();
		if( ! rhs ) {
				// invalid!
			return -1;
		}
		switch( rhs->MyType() ) {
		case LX_STRING:
				// translate the string version to the local number
				// we'll need to use
			name = ((String *)rhs)->Value();
			return signalNumber( name );
			break;
		case LX_INTEGER:
			return ((Integer *)rhs)->Value();
			break;
		default:
			return -1;
			break;
		}
	}
	return -1;
}


int
findSoftKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_KILL_SIG );
}


int
findRmKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_REMOVE_KILL_SIG );
}


} // end of extern "C"
