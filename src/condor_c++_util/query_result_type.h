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
#ifndef _QUERY_RESULT_TYPE_H_
#define _QUERY_RESULT_TYPE_H_

// users of the API must check the results of their calls, which will be one
// of the following
enum QueryResult
{
	Q_OK 					= 0,	// ok
	Q_INVALID_CATEGORY    	= 1,	// category not supported by query type
	Q_MEMORY_ERROR 			= 2,	// memory allocation error within API
	Q_PARSE_ERROR		 	= 3,	// constraints could not be parsed
	Q_COMMUNICATION_ERROR 	= 4,	// failed communication with collector
	Q_INVALID_QUERY			= 5,	// query type not supported/implemented
	Q_NO_COLLECTOR_HOST     = 6		// could not determine collector host
};

#endif
