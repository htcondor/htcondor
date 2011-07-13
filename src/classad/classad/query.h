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


#ifndef __CLASSAD_QUERY_H__
#define __CLASSAD_QUERY_H__

#include <vector>
#include <list>
#include <string>

namespace classad {

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


} // classad

#endif

