/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
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
 *********************************************************************/

#ifndef __QUERY_H__
#define __QUERY_H__

#include <vector>
#include <list>
#include <string>

BEGIN_NAMESPACE( classad )

class ClassAdCollection;
class ExprTree;
class ClassAd;

class LocalCollectionQuery {
public:
	LocalCollectionQuery( );
	~LocalCollectionQuery( );

	void Bind( ClassAdCollection * );
	bool Query( const std::string &viewName, ExprTree *constraint=NULL, 
                bool two_way_matching = false);
	void Clear( );

	void ToFirst(void);
	bool IsAtFirst( ) const { return( itr==keys.begin( ) ); }
	bool Current( std::string &key );
	bool Next( std::string &key );
	bool Prev( std::string &key );
	void ToAfterLast(void);
	bool IsAfterLast(void) const { return( itr==keys.end( ) ); }

	typedef std::vector<std::string>::iterator iterator;
	typedef std::vector<std::string>::const_iterator const_iterator;

	iterator begin()              { return keys.begin(); }
	const_iterator begin() const  { return keys.begin(); }
	iterator end()                { return keys.end(); }
	const_iterator end() const    { return keys.end(); }

private:
	ClassAdCollection                   *collection;
	std::vector<std::string>            keys;
	std::vector<std::string>::iterator  itr;
};


END_NAMESPACE // classad

#endif

