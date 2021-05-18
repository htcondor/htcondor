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


#include "condor_common.h"
#include "HashTable.h"
#include "proc.h"

size_t
hashFuncInt( const int& n )
{
	if( n < 0 ) {
		return (0 - n);
	}
	return n;
}

size_t
hashFuncLong( const long& n )
{
	if( n < 0 ) {
		return (0 - n);
	}
	return n;
}

size_t
hashFuncUInt( const unsigned int& n )
{
	return n;
}

size_t
hashFuncVoidPtr( void* const & pv )
{
   size_t ui = 0;
   for (int ix = 0; ix < (int)(sizeof(void*) / sizeof(int)); ++ix)
      {
      ui += ((unsigned int const*)&pv)[ix];
      }
   return ui;
}

size_t
hashFuncPROC_ID( const PROC_ID &procID )
{
	return ( (procID.cluster+(procID.proc*19)) );
}

// Chris Torek's world famous hashing function
size_t hashFunction( char const *key )
{
    size_t i = 0;
    if ( key ) {
		for ( ; *key ; key++ ) {
			i += (i<<5) + (unsigned char)*key;
		}
    }
    return i;
}

size_t hashFunction( const std::string &key )
{
	return hashFunction( key.c_str() );
}

size_t hashFunction( const MyString &key )
{
	return hashFunction( key.c_str() );
}

size_t hashFunction( const YourString &key )
{
	return hashFunction( key.Value() );
}

size_t hashFunction( const YourStringNoCase &key )
{
    size_t i = 0;
	for ( const char *p = key.Value(); *p ; p++ ) {
		i += (i<<5) + (unsigned char)(*p & ~0x20);
	}
    return i;
}
