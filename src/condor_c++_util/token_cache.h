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
#ifndef __TOKEN_CACHE_H
#define __TOKEN_CACHE_H

#ifdef WIN32

#include "condor_common.h"
#include "HashTable.h"
#include "MyString.h"

#define MAX_CACHE_SIZE 10		// maximum number of tokens to cache. This 
								// might be a config file knob one day.


/* define a couple data structures */

struct token_cache_entry {
	int age;
	HANDLE user_token;
};

typedef HashTable<MyString, token_cache_entry*> TokenHashTable;

/* This class manages our cached user tokens. */

class token_cache {
	public:
		token_cache();
		~token_cache();

		HANDLE getToken(const char* username, const char* domain);
		bool storeToken(const char* username, const char* domain, 
			const HANDLE token);
		MyString cacheToString();		
		
	private:

		/* Data Members */
		int current_age;
		TokenHashTable *TokenTable;
		
		/* helper functions */
		int getNextAge() { return current_age++; }
		void removeOldestToken();
		bool isOlder(int index, int olderIndex) { 
			return ( index < olderIndex); 
		}
		int getCacheSize() { return TokenTable->getNumElements(); }
	
};

#endif // WIN32
#endif // __TOKEN_CACHE_H
