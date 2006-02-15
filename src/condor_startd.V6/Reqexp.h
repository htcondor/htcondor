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
/* 
    This file defines the Reqexp class.  A Reqexp object contains
    methods and data to manipulate the requirements expression of a
    given resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _REQEXP_H
#define _REQEXP_H

enum reqexp_state { UNAVAIL_REQ, ORIG_REQ, COD_REQ };

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
	char*			m_origvalidckptpltfrm;
	reqexp_state	rstate;
};

#endif /* _REQEXP_H */
