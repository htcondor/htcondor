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
    data to hold the requirements expressions of a
    given resource for various modes of operation.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _REQEXP_H
#define _REQEXP_H

#include <string>

enum reqexp_state { NORMAL_REQ, COD_REQ, UNAVAIL_REQ,  };

class Reqexp
{
public:
	std::string		within_resource_limits;
	char* 			origstart{nullptr};
	ExprTree *		drainingStartExpr{nullptr};
	reqexp_state	rstate{NORMAL_REQ};

	~Reqexp() {
		if( origstart ) free( origstart );
		if( drainingStartExpr ) { delete drainingStartExpr; }
		origstart = nullptr;
		drainingStartExpr = nullptr;
	};
};

#endif /* _REQEXP_H */
