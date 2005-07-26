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

#include "condor_common.h"
#include "MyString.h"
#include "condor_config.h"

extern "C" {

/* 
   This converts the given int into a string, and tacks on "st", "nd",
   "rd", or "th", as appropriate.  The value is returned in a static
   buffer, so beware, and use w/ caution.
*/
char*
num_string( int num )
{
	int i;
	static char buf[32];

	i = num % 100; 	// i is just last two digits.
	
		// teens are a special case... they're always "th"
	if( i > 10 && i < 20 ) {
		sprintf( buf, "%dth", num );
		return buf;
	}

	i = i % 10;		// Now, we have just the last digit.
	switch( i ) {
	case 1:
		sprintf( buf, "%dst", num );
		return buf;
		break;
	case 2:
		sprintf( buf, "%dnd", num );
		return buf;
		break;
	case 3:
		sprintf( buf, "%drd", num );
		return buf;
		break;
	default:
		sprintf( buf, "%dth", num );
		return buf;
		break;
	}
	return "";
}


char*
startdClaimIdFile( int vm_id )
{
	MyString filename;

	char* tmp;
	tmp = param( "STARTD_CLAIM_ID_FILE" );
	if( tmp ) {
		filename = tmp;
		free( tmp );
		tmp = NULL;
	} else {
			// otherwise, we must create our own default version...
		tmp = param( "LOG" );
		if( ! tmp ) {
			dprintf( D_ALWAYS,
					 "ERROR: startdClaimIdFile: LOG is not defined!\n" );
			return NULL;
		}
		filename = tmp;
		free( tmp );
		tmp = NULL;
		filename += DIR_DELIM_CHAR;
		filename += ".startd_claim_id";
	}

	if( vm_id ) {
		filename += ".vm";
		filename += vm_id;
	}			
	return strdup( filename.Value() );
}


} // extern "C"
