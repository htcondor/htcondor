/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

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
	void	dprintf( int, const char* ... );

private:
	Resource*		rip;
	char* 			origreqexp;
	char* 			origstart;
	char*			m_origvalidckptpltfrm;
	char*			m_within_resource_limits_expr;
	reqexp_state	rstate;
};

#endif /* _REQEXP_H */
