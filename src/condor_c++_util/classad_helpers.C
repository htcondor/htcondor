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
	string name;
	int sigNum;

	// First, let's see if we just got it as an int.
	if( ad->EvaluateAttrInt( attr_name, sigNum ) ) {
		return sigNum;
	}
	
	// Alright, maybe we get it as a string		
	if( ad->EvaluateAttrString( attr_name, name ) ) {
		return( signalNumber( name.c_str() ) );	
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
