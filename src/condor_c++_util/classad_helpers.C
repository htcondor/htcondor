/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
 
/*
  This file holds utility functions that rely on ClassAds.
*/

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "sig_name.h"

using namespace std;

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


int
findHoldKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_HOLD_KILL_SIG );
}


} // end of extern "C"
