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
    This file defines the Reqexp class.  A Reqexp object contains
    methods and data to manipulate the requirements expression of a
    given resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _REQEXP_H
#define _REQEXP_H

enum reqexp_state { UNAVAIL, ORIG };

class Reqexp
{
public:
	Reqexp( Resource* rip );
	~Reqexp();

	bool	restore();		// Restore the original requirements
	void	unavail();		// Set requirements to False

	void 	publish( ClassAd*, amask_t );
	void	compute( amask_t );
	void	dprintf( int, char* ... );

private:
	Resource*		rip;
	char* 			origreqexp;
	char* 			origstart;
	reqexp_state	rstate;
};

#endif _REQEXP_H
