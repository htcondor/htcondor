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
#include "exit.h"


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


int
findHoldKillSig( ClassAd* ad )
{
	return findSignal( ad, ATTR_HOLD_KILL_SIG );
}


bool
printExitString( ClassAd* ad, int exit_reason, MyString &str )
{
		// first handle a bunch of cases that don't really need any
		// info from the ClassAd at all.

	switch ( exit_reason ) {

	case JOB_KILLED:
		str += "was removed by the user";
		return true;
		break;

	case JOB_NOT_CKPTED:
		str += "was evicted by condor, without a checkpoint";
		return true;
		break;

	case JOB_NOT_STARTED:
		str += "was never started";
		return true;
		break;

	case JOB_SHADOW_USAGE:
		str += "had incorrect arguments to the condor_shadow ";
		str += "(internal error)";
		return true;
		break;

	case JOB_COREDUMPED:
	case JOB_EXITED:
			// these two need more work...  we handle both below
		break;

	default:
		str += "has a strange exit reason code of ";
		str += exit_reason;
		return true;
		break;

	} // switch()

		// if we're here, it's because we hit JOB_EXITED or
		// JOB_COREDUMPED in the above switch.  now we've got to pull
		// a bunch of info out of the ClassAd to finish our task...

	int int_value;
	bool exited_by_signal = false;
	int exit_value = -1;

		// first grab everything from the ad we must have for this to
		// work at all...
	if( ad->LookupBool(ATTR_ON_EXIT_BY_SIGNAL, int_value) ) {
		if( int_value ) {
			exited_by_signal = true;
		} else {
			exited_by_signal = false;
		}
	} else {
		dprintf( D_ALWAYS, "ERROR in printExitString: %s not found in ad\n",
				 ATTR_ON_EXIT_BY_SIGNAL );
		return false;
	}

	if( exited_by_signal ) {
		if( ad->LookupInteger(ATTR_ON_EXIT_SIGNAL, int_value) ) {
			exit_value = int_value;
		} else {
			dprintf( D_ALWAYS, "ERROR in printExitString: %s is true but "
					 "%s not found in ad\n", ATTR_ON_EXIT_BY_SIGNAL,
					 ATTR_ON_EXIT_SIGNAL);
			return false;
		}
	} else { 
		if( ad->LookupInteger(ATTR_ON_EXIT_CODE, int_value) ) {
			exit_value = int_value;
		} else {
			dprintf( D_ALWAYS, "ERROR in printExitString: %s is false but "
					 "%s not found in ad\n", ATTR_ON_EXIT_BY_SIGNAL,
					 ATTR_ON_EXIT_CODE);
			return false;
		}
	}

		// now we can grab all the optional stuff that might help...
	char* ename = NULL;
	int got_exception = ad->LookupString(ATTR_EXCEPTION_NAME, &ename); 
	char* reason_str = NULL;
	ad->LookupString( ATTR_EXIT_REASON, &reason_str );

		// finally, construct the right string
	if( exited_by_signal ) {
		if( got_exception ) {
			str += "died with exception ";
			str += ename;
		} else {
			if( reason_str ) {
				str += reason_str;
			} else {
				str += "died on signal ";
				str += exit_value;
			}
		}
	} else {
		str += "exited normally with status ";
		str += exit_value;
	}

	if( ename ) {
		free( ename );
	}
	if( reason_str ) {
		free( reason_str );
	}		
	return true;
}
