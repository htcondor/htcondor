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

} // extern "C"
