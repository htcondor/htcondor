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
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#if !defined(WIN32)
#include <netinet/in.h>
#endif

#include "condor_query.h"
#include "condor_attributes.h"
#include "condor_collector.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_config.h"

CondorQuery::
CondorQuery( )
{
	domain = (AdDomain)-1;
	adType = (CondorAdType)NULL;
}


CondorQuery::
CondorQuery( CondorAdType adt, AdDomain add )
{
	setQueryType( adt, add );
}


CondorQuery::
CondorQuery( const CondorQuery &from ) : GenericQuery( from )
{
	domain = from.domain;
	adType = from.adType;
}


// dtor
CondorQuery::
~CondorQuery ()
{	
}


// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds( ExprList  &adList, const char *poolName )
{
    char  		*pool;
	char		defaultPool[64];
    ReliSock    sock; 
    QueryResult result;
    ClassAd     queryAd;
	int			command;
	Source		source;
	Sink		sink;

		// ensure that we have enough state to make the query
	if( domain == (AdDomain)-1 || adType == (CondorAdType)NULL ) {
		return( Q_INVALID_QUERY );
	}

		// use current pool's collector if not specified
	if( poolName == NULL || poolName[0] == '\0' ) {
		if( ( pool = param("COLLECTOR_HOST") ) == NULL ) {
			return Q_NO_COLLECTOR_HOST;
		}
		strcpy( defaultPool, pool );
		free( (void*)pool );
		pool = defaultPool;
	} else {
			// pool specified
		pool = (char *) poolName;
	}

		// make the query ad; add type information
	if( !makeQueryAd( queryAd ) || !queryAd.insertAttr( ATTR_TYPE, adType ) ) {
		return( Q_MEMORY_ERROR );
	}

		// the command that we have to send to the collector
	switch( domain ) {
		case PUBLIC_AD: 
			command = QUERY_PUBLIC_ADS; 
			break;
		case PRIVATE_AD: 
			command = QUERY_PRIVATE_ADS; 
			break;
		default: 
			return( Q_INVALID_QUERY );
	}

		// contact collector
	if( !sock.connect( pool, COLLECTOR_COMM_PORT ) ) {
        return Q_COMMUNICATION_ERROR;
    }

		// ship query
	sink.setSink( sock );
	sock.encode( );
	if( !sock.code(command) || !queryAd.toSink(sink) || 
		!sink.flushSink( )	|| !sock.end_of_message( ) ) {
		return( Q_COMMUNICATION_ERROR );
	}

		// get result
	sock.decode( );
	source.setSource( sock );
	if( !source.parseExprList( &adList ) ) {
		return( Q_COMMUNICATION_ERROR );
	}

		// finalize
	sock.end_of_message();
	sock.close ();
	
	return (Q_OK);
}


QueryResult CondorQuery::
filterAds( ExprList &in, ExprList &out )
{
	ClassAd 		queryAd, *candidate;
	QueryResult		result;
	Value			val;
	CondorClassAd	ad;
	ExprTree		*tree;
	bool			match;

		// make the query ad
	if( !makeQueryAd( queryAd ) ) {
		return( Q_MEMORY_ERROR );
	}

		// plug in the query into the left context
	ad.replaceLeftAd( &queryAd );

	in.rewind( );
	while( ( tree = in.next( ) ) ) {
			// 'tree' better be a classad
		if( !tree->evaluate( val ) || !val.isClassAdValue( candidate ) ) {
			return( Q_INVALID_QUERY_RESULT );
		}
			// plug in the candidate and test for a match
		ad.replaceRightAd( candidate );
		if( ad.evaluateAttrBool( "rightMatchesLeft", match ) && match ) {
			out.appendExpression( candidate->copy( ) );
		}
    }
    
	return Q_OK;
}


char *getStrQueryResult(QueryResult q)
{
	switch (q)
	{
    	case Q_OK:					return "ok";
    	case Q_INVALID_CATEGORY:	return "invalid category";
    	case Q_MEMORY_ERROR:		return "memory error";
    	case Q_PARSE_ERROR:			return "parse error";
	    case Q_COMMUNICATION_ERROR:	return "communication error";
	    case Q_INVALID_QUERY:		return "invalid query";
	    case Q_NO_COLLECTOR_HOST:	return "no COLLECTOR_HOST";
		case Q_INVALID_QUERY_RESULT:return "invalid query result";
		default:
			return "unknown error";
	}
}
