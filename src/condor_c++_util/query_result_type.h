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
