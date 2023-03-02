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


#ifndef LSA_MGR_H
#define LSA_MGR_H

#include "condor_common.h"

#ifdef WIN32

#include <ntsecapi.h>

#define CC_DATA_DELIM L'|' 		// separates data members in a record
#define CC_RECORD_DELIM L'\n'	// separates different records

// Registry key name to store passwords under
// L$ makes it local, M$ makes it machine, G$ makes it global
#define CONDOR_PASSWORD_KEYNAME L"L$Condor_pw655321" 

int interactive(void); // forward decl

class lsa_mgr {
	public:

		lsa_mgr(); // constructor
		~lsa_mgr(); // destructor

		//------------------
		// management methods
		//------------------

		// add new password to registry
		// returns true on successful add
		// fails if Owner:Login already exists
		bool add( const LPWSTR Login, const LPWSTR Passwd );

		// remove passwd, assuming what is passed matches what is to be removed.
		// returns true on successful remove
		bool remove( const LPWSTR Login );

		// returns true if we have a password stored for login
		bool isStored( const LPWSTR Login );

		// return user's password or NULL if no
		// matching record exists
		// You MUST delete the result!
		LPWSTR query( const LPWSTR Login );

		//--------------------
		// convenience methods
		//--------------------

		// prints all entries to stdout
		void printAllData();

		// Blows away everything...be careful!
		bool purgeAllData();
		
		// convert char to unicode string
		LPWSTR charToUnicode( char* str ); 
		// basically a wide-char strstr that's case insensitive
		wchar_t* wcsstri(wchar_t* haystack, wchar_t* needle);

	private:
		

		// convert unicode to LSA_UNICODE_STRING object
		void InitLsaString( PLSA_UNICODE_STRING LsaString, PCWSTR String );

		// load all password data from registry
		bool loadDataFromRegistry();
		
		// store password data from registry
		bool storeDataToRegistry( PLSA_UNICODE_STRING lsaString );

		// copies DataBuffer into a new, null-terminated buffer at Data_string
		void extractDataString();

		// returns a pointer to the location in Data_string that matches the
		// given Owner, Login pair or null if no match
		LPWSTR findInDataString( const LPWSTR Login, bool case_sense=false );

		// cleans up buffers used by this class to store password info
		void freeBuffers() { 
			if ( DataBuffer ) {
				LsaFreeMemory(DataBuffer);
				DataBuffer = NULL;
			}
			if ( Data_string ) {
				if ( wcslen(Data_string) > 0 ) {
					SecureZeroMemory(Data_string, sizeof(WCHAR)*wcslen(Data_string));
				}
				delete[] Data_string;
				Data_string = NULL;
			}
		};
	

		//
		// data members
		
		PLSA_UNICODE_STRING DataBuffer; // ptr to buffer as returned by win32 LSA call
		LPWSTR Data_string;	// ptr to raw string that contains the data from reg.
};

#endif // WIN32
#endif // LSA_MGR_H
