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
