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
#ifndef __CONDOR_QUERY_H__
#define __CONDOR_QUERY_H__

#include "condor_classad.h"
#include "condor_collector.h"
#include "condor_attributes.h"
#include "condor_adtypes.h"
#include "query_result_type.h"
#include "generic_query.h"

class CondorQuery : public GenericQuery
{
  public:
		// ctor/dtor
	CondorQuery( );
	CondorQuery( CondorAdType, AdDomain=PUBLIC_AD );
	CondorQuery( const CondorQuery & );
	~CondorQuery( );

		// set query type
	QueryResult setQueryType( CondorAdType, AdDomain=PUBLIC_AD );

		// fetch from collector
	QueryResult fetchAds( ExprList &, const char* pool=NULL );

		// filter list of ads with query
	QueryResult filterAds( ExprList &in, ExprList &out );

  private:
	AdDomain		domain;
	CondorAdType	adType;
};

#endif
