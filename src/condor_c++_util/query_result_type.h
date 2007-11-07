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
