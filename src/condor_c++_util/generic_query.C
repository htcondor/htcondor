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
#include "generic_query.h"

static char *_FileName_ = __FILE__;

GenericQuery::
GenericQuery( )
{
	if( ( collIdentHints = new ClassAd( ) ) == NULL ) {
		EXCEPT( "Out of memory" );
	}
	collIdHints=false;
	constraint= NULL;
	project   = NULL;
	rankHints = NULL;
	preamble  = true;
	results   = true;
	summary   = true;
}

GenericQuery::
GenericQuery( const GenericQuery& gq )
{
	if( collIdentHints ) delete collIdentHints;
	collIdentHints = gq.collIdentHints ? gq.collIdentHints->Copy( ) : NULL;
	collIdHints=gq.collIdHints;
	constraint= gq.constraint ? gq.constraint->Copy( ) : NULL;
	project   = gq.project ? gq.project->Copy( ) : NULL;
	rankHints = gq.rankHints ? gq.rankHints->Copy( ) : NULL;
	preamble  = gq.preamble;
	results   = gq.results;
	summary   = gq.summary;
}

GenericQuery::
~GenericQuery( )
{
	if( constraint ) delete constraint;
	if( project ) delete project;
	if( collIdentHints ) delete collIdentHints;
	if( rankHints ) delete rankHints;
}

void GenericQuery::
clearConstraint( )
{
	delete constraint;
	constraint = NULL;
}

int GenericQuery::
addConstraint( const char* expr, OpKind op )
{
	ExprTree *tree;
	src.SetSource( expr );
	if( !src.ParseExpression( tree ) ) return( Q_PARSE_ERROR );
	return( addConstraint( tree, op ) );
}

int GenericQuery::
addConstraint( ExprTree *tree, OpKind op )
{
	if( op!=LOGICAL_AND_OP && op!=LOGICAL_NOT_OP ) return( Q_INVALID_QUERY );

	Operation *newConstraint = new Operation( );
	if( !newConstraint ) return( Q_MEMORY_ERROR );
	newConstraint->SetOperation( op, constraint, tree );
	constraint = newConstraint;

	return( Q_OK );
}

void GenericQuery::
setConstraint( ExprTree * tree ) 
{
	if( constraint ) delete constraint;
	constraint = tree;
}

void GenericQuery::
clearProjectionAttrs( )
{
	if( project ) delete project;
	project = NULL;
}


void GenericQuery::
addProjectionAttr( const char* attr )
{
	Literal *newAttr;
	char 	*str;

	if( !project && ( ( project = new ExprList ) == NULL ) ) {
		EXCEPT( "Out of memory" );
	}
	if( ( str = strnewp( attr ) ) == NULL ) {
		EXCEPT( "Out of memory" );
	}
	if( ( newAttr = new Literal( ) ) == NULL ) {
		EXCEPT( "Out of memory" );
	}
	newAttr->SetStringValue( str );
	project->AppendExpression( newAttr );
}


void GenericQuery::
setProjectionAttrs( ExprList *el ) 
{
	if( project ) delete project;
	project = el;
}


void GenericQuery::
clearCollIdentHints( )
{
	collIdentHints->Clear( );
	collIdHints = false;
}


void GenericQuery::
addCollIdentHint( const char* attr, ExprTree* hint )
{
	if( !collIdentHints->Insert( attr, hint ) ) {
		EXCEPT( "Failed to add hint" );
	}
	collIdHints = true;
}

void GenericQuery::
addCollIdentHint( const char* attr, int hint )
{
	if( !collIdentHints->InsertAttr( attr, hint ) ) {
		EXCEPT( "Failed to add hint" );
	}
	collIdHints = true;
}

void GenericQuery::
addCollIdentHint( const char* attr, double hint )
{
	if( !collIdentHints->InsertAttr( attr, hint ) ) {
		EXCEPT( "Failed to add hint" );
	}
	collIdHints = true;
}

void GenericQuery::
addCollIdentHint( const char* attr, const char*hint )
{
	if( !collIdentHints->InsertAttr( attr, hint ) ) {
		EXCEPT( "Failed to add hint" );
	}
	collIdHints = true;
}


void GenericQuery::
addCollIdentHint( const char* attr, bool hint )
{
	if( !collIdentHints->InsertAttr( attr, hint ) ) {
		EXCEPT( "Failed to add hint" );
	}	
	collIdHints = true;
}


void GenericQuery::
setCollIdentHints( ClassAd* hints )
{
	if( collIdentHints ) delete collIdentHints;
	collIdentHints = hints;
	collIdHints = true;
}

void GenericQuery::
clearRankHints( )
{
	if( rankHints ) delete rankHints;
	rankHints = NULL;
}


void GenericQuery::
addRankHint( const char* attr )
{
	Literal *newAttr;
	char 	*str;

	if( !rankHints && ( ( rankHints = new ExprList ) == NULL ) ) {
		EXCEPT( "Out of memory" );
	}
	if( ( str = strnewp( attr ) ) == NULL ) {
		EXCEPT( "Out of memory" );
	}
	if( ( newAttr = new Literal( ) ) == NULL ) {
		EXCEPT( "Out of memory" );
	}
	newAttr->SetStringValue( str );
	rankHints->AppendExpression( newAttr );
}


void GenericQuery::
setRankHints( ExprList *el ) 
{
	if( rankHints ) delete rankHints;
	rankHints = el;
}


void GenericQuery::
wantPreamble( bool p )
{
	preamble = p;
}


void GenericQuery::
wantResults( bool r )
{
	results = r;
}


void GenericQuery::
wantSummary( bool s )
{
	summary = s;
}


ClassAd* GenericQuery::
makeQueryAd( )
{
	ClassAd *ad = new ClassAd;
	if( !ad ) return( NULL );
	if( !makeQueryAd( *ad ) ) {
		delete ad;
		return( NULL );
	}
	return( ad );
}


bool GenericQuery::
makeQueryAd( ClassAd &ad )
{
	Literal *lit;
		// if a constraint is not specified assume "true"
	if( !constraint ) {
		if( ( lit = new Literal( ) ) == NULL ) {
			return( false );
		}
		lit->SetBooleanValue( true );
		if( !ad.Insert( ATTR_REQUIREMENTS, constraint->Copy( ) ) ) {
			return( false );
		}
	} else if( !ad.Insert( ATTR_REQUIREMENTS, constraint->Copy( ) ) ) {
		return( false );
	}

		// if no projection attributes, don't Insert ATTR_PROJ_ATTRS
	if( project && !ad.Insert( ATTR_PROJ_ATTRS, project->Copy( ) ) ) {
		return( false );
	}

		// if no collection identification hints, don't Insert ATTR_COLL_HINTS
	if( collIdHints ) {
		if( !ad.Insert( ATTR_COLL_HINTS, collIdentHints->Copy( ) ) ) {
			return( false );
		}
	}

		// if no rank hints, don't Insert ATTR_RANK_HINTS
	if( rankHints && !ad.Insert( ATTR_RANK_HINTS, rankHints->Copy( ) ) ) {
		return( false );
	}

		// required parts of the query
	if( !ad.InsertAttr( ATTR_WANT_PREAMBLE, preamble ) ||
		!ad.InsertAttr( ATTR_WANT_RESULTS, results ) ||
		!ad.InsertAttr( ATTR_WANT_SUMMARY, summary ) ) {
			return false;
	}
	return( true );
}

