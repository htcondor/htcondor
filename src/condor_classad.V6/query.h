/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2001, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#if !defined( QUERY_H )
#define QUERY_H

#include <vector>
#include <list>
#include <string>

BEGIN_NAMESPACE( classad );

class ClassAdCollection;
class ExprTree;
class ClassAd;

class LocalCollectionQuery {
public:
	LocalCollectionQuery( );
	~LocalCollectionQuery( );

	void Bind( ClassAdCollection * );
	bool Query( const string &viewName, ExprTree *constraint=NULL );
	void Clear( );

	void ToFirst(void);
	bool IsAtFirst( ) const { return( itr==keys.begin( ) ); }
	bool Current( string &key );
	bool Next( string &key );
	bool Prev( string &key );
	void ToAfterLast(void);
	bool IsAfterLast(void) const { return( itr==keys.end( ) ); }

private:
	ClassAdCollection		    *collection;
	vector<string>				keys;
	vector<string>::iterator	itr;
};


END_NAMESPACE // classad

#endif

