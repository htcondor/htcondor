/***************************************************************
 *
 * Copyright 2012 Red Hat, Inc. 
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

#ifndef ___UNORDERED_MAP_H__
#define ___UNORDERED_MAP_H__

#include "config.h" 

#ifdef PREFER_CPP11
	#include <unordered_map>
	#define _unordered_map std::unordered_map
#elif defined(PREFER_TR1)
	#include <tr1/unordered_map>
	#define _unordered_map std::tr1::unordered_map
#else
	#include <boost/unordered_map.hpp>
	#define _unordered_map boost::unordered_map
#endif

#include <string>
#ifdef WIN32
  #include <string.h>
  #define strcasecmp _stricmp
#else
  #include <strings.h>
#endif

//////////////////////////////////////////////////////////////////////////////////////
//
//  The following are string functions for hash functions ~= classads.
//
//  Depending on your usage you will likely want to do: 
//
//  typedef _unordered_map<std::string, T, _hash_ign_case, _streq_ign_case> _unordered_str_map;
//
//////////////////////////////////////////////////////////////////////////////////////

///< ~= case insensitive string comparison 
struct _streq_ign_case 
{
	inline bool operator()( const std::string &s1, const std::string &s2 ) const 
	{
		return( strcasecmp( s1.c_str( ), s2.c_str( ) ) == 0 );
	}
};

///< ~=classad hash f(n) 
struct _hash_ign_case
{
	inline size_t operator()( const std::string &s ) const {
		unsigned long h = 0;
		char const *ch;
		for( ch = s.c_str(); *ch; ch++ ) {
			h = 5*h + (unsigned char)tolower(*ch);
		}
		return (size_t)h;
	}

};

#endif
