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

// error codes used by the condor_q schedd query wrapper object
// These must not use the same numbers as the QueryResult codes above.
enum
{
	Q_NO_SCHEDD_IP_ADDR = 20,	   // lookup of ATTR_SCHEDD_IP_ADDR in schedd ad failed
	Q_SCHEDD_COMMUNICATION_ERROR,  // ConnnectQ failed. see errstack for details
	Q_INVALID_REQUIREMENTS,		   // constraint not supplied when required, or constraint did not parse
	Q_INTERNAL_ERROR,
	Q_REMOTE_ERROR,				   // response from the schedd does not have a valid data
	Q_UNSUPPORTED_OPTION_ERROR	   // the combination of options are not supported, trying to use modern options with ConnnectQ
};

// option flags for fetchQueueFromHost* functions, these can modify the meaning of attrs
// use only one of the choices < fetch_FromMask, optionally OR'd with one or more fetch flags
// currently only fetch_Jobs accepts flags.
enum QueryFetchOpts {
	fetch_Jobs=0,
	fetch_DefaultAutoCluster=1,
	fetch_GroupBy=2,
	fetch_FromMask=0x03,         // mask off the 'from' bits
	fetch_MyJobs=0x04,           // modifies fetch_Jobs
	fetch_SummaryOnly=0x08,      // modifies fetch_Jobs
	fetch_IncludeClusterAd=0x10, // modifies fetch_Jobs
	fetch_IncludeJobsetAds=0x20, // modifies fetch_Jobs
	fetch_NoProcAds=0x40,
	fetch_ClusterAds=fetch_IncludeClusterAd | fetch_NoProcAds,
	fetch_JobsetAds=fetch_IncludeJobsetAds | fetch_NoProcAds,
};



#endif
