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
hashFuncJobIdStr( char* const & key )
{
    unsigned int bkt = 0;
	int i,j,size;
    unsigned int multiplier = 1;

    if (key) {
        size = strlen(key);
        for (i=0; i < size; i++) {
            j = size - 1 - i;
            if (key[j] == '.' ) continue;
            bkt += (key[j] - '0') * multiplier;
            multiplier *= 10;
        }
    }

    return bkt;
}

unsigned int 
hashFuncPROC_ID( const PROC_ID &procID )
{
	return ( (procID.cluster+(procID.proc*19)) );
}

unsigned int
hashFuncChars( char const *key )
{
    unsigned int i = 0;
    if(key) for(;*key;key++) {
        i += *(const unsigned char *)key;
    }
    return i;
}

unsigned int hashFuncMyString( const MyString &key )
{
    return hashFuncChars(key.Value());
}
