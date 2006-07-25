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
		void InitLsaString( PLSA_UNICODE_STRING LsaString, const LPWSTR String );

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
				ZeroMemory(Data_string, sizeof(WCHAR)*wcslen(Data_string));
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
