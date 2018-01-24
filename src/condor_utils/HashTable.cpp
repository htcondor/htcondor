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

unsigned int
hashFuncInt( const int& n )
{
	if( n < 0 ) {
		return (0 - n);
	}
	return n;
}

unsigned int
hashFuncLong( const long& n )
{
	if( n < 0 ) {
		return (0 - n);
	}
	return n;
}

unsigned int
hashFuncUInt( const unsigned int& n )
{
	return n;
}

unsigned int 
hashFuncVoidPtr( void* const & pv )
{
   unsigned int ui = 0;
   for (int ix = 0; ix < (int)(sizeof(void*) / sizeof(int)); ++ix)
      {
      ui += ((unsigned int const*)&pv)[ix];
      }
   return ui;
}

unsigned int 
hashFuncPROC_ID( const PROC_ID &procID )
{
	return ( (procID.cluster+(procID.proc*19)) );
}

// Chris Torek's world famous hashing function
unsigned int hashFunction( char const *key )
{
    unsigned int i = 0;
    if ( key ) {
		for ( ; *key ; key++ ) {
			i += (i<<5) + (const unsigned char)*key;
		}
    }
    return i;
}

unsigned int hashFunction( const std::string &key )
{
	return hashFunction( key.c_str() );
}

unsigned int hashFunction( const MyString &key )
{
	return hashFunction( key.Value() );
}

unsigned int hashFunction( const YourString &key )
{
	return hashFunction( key.Value() );
}

unsigned int hashFunction( const YourStringNoCase &key )
{
    unsigned int i = 0;
	for ( const char *p = key.Value(); *p ; p++ ) {
		i += (i<<5) + (const unsigned char)(*p & ~0x20);
	}
    return i;
}
