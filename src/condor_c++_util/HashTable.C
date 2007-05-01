/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
