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
#ifndef __GENERIC_QUERY_H__
#define __GENERIC_QUERY_H__

#include "condor_classad.h"
#include "query_result_type.h"	

class GenericQuery
{
  public:
		// ctor/dtor
	GenericQuery( );
	GenericQuery( const GenericQuery & );
	virtual ~GenericQuery( );
	
		// constraints
	void clearConstraint( );
	int  addConstraint( const char *expr, OpKind op=LOGICAL_OR_OP );
	int  addConstraint( ExprTree *expr, OpKind op=LOGICAL_OR_OP );
	void setConstraint( ExprTree *expr );

		// projection attributes
	void clearProjectionAttrs( );
	void addProjectionAttr( const char* );
	void setProjectionAttrs( ExprList * );

		// hints for collection identification
	void clearCollIdentHints( );
	void addCollIdentHint( const char*, ExprTree* );
	void addCollIdentHint( const char*, int );
	void addCollIdentHint( const char*, double );
	void addCollIdentHint( const char*, const char* );
	void addCollIdentHint( const char*, bool );
	void setCollIdentHints( ClassAd* );

		// hints for ranking
	void clearRankHints( );
	void addRankHint( const char* );
	void setRankHints( ExprList * );

		// required results
	void wantPreamble( bool=true );
	void wantResults( bool=true );
	void wantSummary( bool=true );

		// create the query ad
	ClassAd *makeQueryAd( );
	bool makeQueryAd( ClassAd& );

  protected:
	ExprTree *constraint;
	ExprList *project;
	ClassAd  *collIdentHints;
	ExprList *rankHints;
	bool	 preamble, results, summary;
	Source	 src;

  private:
	bool 	collIdHints;
};

#endif
