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

#ifdef WIN32

#include "lsa_mgr.h"

//----------------------------------
// class lsa_mgr
// 
// manages password storage on win32
// You must be Local System in order
// to run this code successfully.
//----------------------------------


//-----------------
// public methods
//-----------------

lsa_mgr::lsa_mgr() {
	Data_string = NULL;
	DataBuffer = NULL;
}

lsa_mgr::~lsa_mgr() {
	freeBuffers();
}


// careful with this one...it prints out everything in the clear
// It's really just for debugging purposes
void 
lsa_mgr::printAllData() {
	if ( loadDataFromRegistry() ) {

		printf("%S\n", (this->Data_string) ? this->Data_string : L"Null");
		freeBuffers(); // free buffers
	} else {
		printf("No data to print!\n");
	}
}

// careful with this one too--it clears all passwords
bool
lsa_mgr::purgeAllData() {

	LSA_HANDLE policyHandle;
	LSA_UNICODE_STRING keyName;
	NTSTATUS ntsResult;
	LSA_OBJECT_ATTRIBUTES obj_attribs;

	// Object attributes are reserved, so initialize to zeros
	ZeroMemory(&obj_attribs, sizeof(obj_attribs));
	
	// first open a policy handle 
	ntsResult = LsaOpenPolicy(
			NULL, 							// machine name or NULL for local
			&obj_attribs,					// object attributes (?)
			POLICY_CREATE_SECRET,			// policy rights
			&policyHandle					// policy handle ptr
		);

	if (ntsResult != ERROR_SUCCESS) {
		wprintf(L"LsaOpenPolicy returned %lu\n", LsaNtStatusToWinError(ntsResult));
		return NULL;
	}

	// init keyname we want to erase
	InitLsaString( &keyName, CONDOR_PASSWORD_KEYNAME );
	
	// now we can (finally) do the actual delete
	ntsResult = LsaStorePrivateData(
				policyHandle, 		/* LSA_HANDLE */
				&keyName,			/* LSA_UNICODE_STRING registry key name */
				NULL				/* Passing NULL to erase the key */
		);
	
	// be tidy with the silly policy handles
	LsaClose(policyHandle);

	return (ntsResult == ERROR_SUCCESS);
}

// an attempt to add a Login that already exists will
// result in a failure
bool
lsa_mgr::add( const LPWSTR Login, const LPWSTR Passwd ) {  

	wchar_t* new_buffer = NULL; 
	LSA_UNICODE_STRING lsa_new_data;
	bool result = false;

	// sanity checks for input strings
	if ( !Login || 0 == wcslen(Login) ) {
		printf("Must specify a Login!\n");
		return false;
	}else if ( !Passwd || 0 == wcslen(Passwd) ) {
		printf("Must specify a Password!\n");
		return false;
	}

	int new_buffer_len = 	wcslen(Login) + 1 + // Login delimiter
							wcslen(Passwd) + 3;  // newline + null terminator

	if ( loadDataFromRegistry() ) { // if there's already password information in the registry
		if ( findInDataString(Login) ) { // check for a duplicate
			
			// if we find the login is already in the stash, nuke it
			
			freeBuffers();				// clean up buffers so we don't leak memory
			remove(Login);				// nuke the login
			loadDataFromRegistry();		// now reload the data
		}


		new_buffer_len += wcslen(this->Data_string);
		new_buffer = new wchar_t[new_buffer_len];
		wcscpy(new_buffer, this->Data_string);
	} else {
		new_buffer = new wchar_t[new_buffer_len];	
		ASSERT(new_buffer);
		new_buffer[0] = CC_RECORD_DELIM; // init with empty record
		new_buffer[1] = L'\0'; // 
	}

	wcscat(new_buffer, Login );
	new_buffer[wcslen(new_buffer)+1] = L'\0';
	new_buffer[wcslen(new_buffer)] = CC_DATA_DELIM;

	wcscat(new_buffer, Passwd );
	new_buffer[wcslen(new_buffer)+1] = L'\0';
	new_buffer[wcslen(new_buffer)] = CC_RECORD_DELIM;

	//first close up the data buffer if it's open already
	freeBuffers();
	
	// prepare new data buffer
	InitLsaString(&lsa_new_data, new_buffer);

	result = storeDataToRegistry(&lsa_new_data);
	
	if (! result ) {
		printf("lsa_mgr::storeDataToRegistry() failed!\n");
	}

	// clean up the new buffer after its stored
	ZeroMemory(new_buffer, sizeof(WCHAR)*wcslen(new_buffer));
	delete[] new_buffer;

	return result;
}

bool
lsa_mgr::remove( const LPWSTR Login ) {
	
	LSA_UNICODE_STRING newBuffer; // new data (with stuff removed) to store in registry
	int remove_len; // size of record to remove from stash
	wchar_t* newData; // string containing new data


	if ( loadDataFromRegistry() ) {
		
		// first find out where the record is that we want to remove
		wchar_t* result = findInDataString( Login );

		if ( result ) {
			// now if we found something, move past the first delimiter
			result++; 
			
			// now calculate how much we have to skip over when we rewrite the data buffer to 
			// the registry
			remove_len = wcschr(result, CC_RECORD_DELIM) - (result); // how many chars to remove
			remove_len++; // remove the extra newline too

			// create a new buffer and copy everything we want to keep into it
			newData = new wchar_t[(wcslen(Data_string) - remove_len) +1];
			
			// copy everything up to recrd
			wcsncpy(newData, Data_string, (result - Data_string) ); 
			newData[(result - Data_string)] = L'\0';

			// now skip over the record we're removing
			result += remove_len;
			
			// and copy what comes after it
			wcscat(newData, result);

			// finally, store it to the registry and clean up
			InitLsaString( &newBuffer, newData );
			storeDataToRegistry( &newBuffer );

			delete[] newData;
			freeBuffers();
			return true;
		}

		freeBuffers();
		return false;
	} else {
		return false; // no data in registry
	}
}

bool
lsa_mgr::isStored( const LPWSTR Login ) {
	wchar_t* pw = NULL;

	pw = query(Login);
	if ( pw ) {
		// we found something, but don't leak memory
		ZeroMemory(pw, wcslen(pw));
		delete[] pw;
		return true;
	} else {
		return false;
	}
}

LPWSTR
lsa_mgr::query( const LPWSTR Login ) {
	
	wchar_t* pw;	// pointer to new buffer containing requested password
	int pwlen;		// length of password

	if ( loadDataFromRegistry() ) {
	
		int query_str_size = wcslen(Login) + 2; // delimiter+null

		// locate login and peel off the password part
		wchar_t* result = findInDataString( Login );
		
		if ( result ) {
			result += query_str_size; // move ptr to password part
			pwlen = wcschr(result+1, CC_RECORD_DELIM) - result;
			pw = new wchar_t[pwlen+1];
			wcsncpy(pw, result, pwlen);
			pw[pwlen] = L'\0';  // make sure it's null terminated!
			return pw;
		} else { 
			return NULL;
		}
	} else { return NULL; } // this happens if there's no data to retrieve from registry
}

// convert char to unicode. You must free what is returned!
LPWSTR
lsa_mgr::charToUnicode( char* str ) {
	LPWSTR str_unicode = new wchar_t[strlen(str)+1];
	MultiByteToWideChar(CP_ACP, 0, str, -1, str_unicode, strlen(str)+1);
	return str_unicode;
}

//-----------------
// private methods
//-----------------

bool 
lsa_mgr::loadDataFromRegistry() {

	LSA_HANDLE policyHandle;
	LSA_UNICODE_STRING keyName;
	NTSTATUS ntsResult;
	LSA_OBJECT_ATTRIBUTES obj_attribs;

	// Object attributes are reserved, so initialize to zeros
	ZeroMemory(&obj_attribs, sizeof(obj_attribs));
	
	// first open a policy handle 
	ntsResult = LsaOpenPolicy(
			NULL, 							// machine name or NULL for local
			&obj_attribs,					// object attributes (?)
			POLICY_GET_PRIVATE_INFORMATION, // policy rights
			&policyHandle					// policy handle ptr
		);

	if (ntsResult != ERROR_SUCCESS) {
		wprintf(L"LsaOpenPolicy returned %lu\n", LsaNtStatusToWinError(ntsResult));
		return NULL;
	}

	// init keyname we want to grab
	InitLsaString( &keyName, CONDOR_PASSWORD_KEYNAME );
	
	// now we can (finally) grab the private data
	ntsResult = LsaRetrievePrivateData(
				policyHandle, 		/* LSA_HANDLE */
				&keyName,			/* LSA_UNICODE_STRING registry key name */
				&DataBuffer 		/* LSA_UNICODE_STRING private data */
		);

	// be tidy with the silly policy handle 
	LsaClose(policyHandle);

	if (ntsResult == ERROR_SUCCESS) {
		
		// decrypt our data so we can read it, but be careful...
		// we may not have to decrypt it if users have stored 
		// the passwords with a pre 6.6.3 version of Condor,
		// (but next time we store it, it'll be encrypted)
		
		DATA_BLOB DataIn, DataOut;
		LPWSTR pDescrOut =  NULL;

		DataOut.pbData = NULL;
		DataIn.pbData = (BYTE*)DataBuffer->Buffer;
		DataIn.cbData = DataBuffer->Length;

		if (!CryptUnprotectData(
			&DataIn,
			&pDescrOut,			// Description string
			NULL,				// Optional Entropy,
			NULL,				// Reserved
			NULL,				// optional promptstruct
			CRYPTPROTECT_UI_FORBIDDEN, // No GUI prompt!
			&DataOut)){
			
			DWORD err = GetLastError();

			if ( err == ERROR_PASSWORD_RESTRICTION ) {

				// this means the password wasn't encypted 
				// when we got it, so do nothing, and pass
				// the data on to extractDataString() 
				// untouched.

			} else {

				// this means decryption failed for some
				// other reason, so return failure.
				return false;
			}
			
		} else {
			DataBuffer->Buffer = (PWSTR) DataOut.pbData;
			DataBuffer->Length = (USHORT) DataOut.cbData;
		}
		
		extractDataString();

		if ( DataOut.pbData != NULL ) {
			LocalFree(DataOut.pbData);
		}

		if ( pDescrOut != NULL ) {
			LocalFree(pDescrOut);
		}

		return true;
	} else {
		return false;
	}
}

bool 
lsa_mgr::storeDataToRegistry( const PLSA_UNICODE_STRING lsaString ) {

	LSA_HANDLE policyHandle;
	LSA_UNICODE_STRING keyName;
	NTSTATUS ntsResult;
	LSA_OBJECT_ATTRIBUTES obj_attribs;

	// Object attributes are reserved, so initialize to zeros
	ZeroMemory(&obj_attribs, sizeof(obj_attribs));
	
	// first open a policy handle 
	ntsResult = LsaOpenPolicy(
			NULL, 							// machine name or NULL for local
			&obj_attribs,					// object attributes (?)
			POLICY_CREATE_SECRET,			// policy rights
			&policyHandle					// policy handle ptr
		);

	if (ntsResult != ERROR_SUCCESS) {
		dprintf(D_ALWAYS, "LsaOpenPolicy returned %lu\n", 
			LsaNtStatusToWinError(ntsResult));
		return NULL;
	}

	// init keyname we want to grab
	InitLsaString( &keyName, CONDOR_PASSWORD_KEYNAME );

	// Encrypt data before storing it
	DATA_BLOB DataIn, DataOut;

	DataOut.pbData = NULL;
	DataIn.pbData = (BYTE*) lsaString->Buffer;
	DataIn.cbData = lsaString->Length;

	if(!CryptProtectData(
        &DataIn,
        L"Condor",			// A description sting. 
        NULL,				// Optional entropy not used
        NULL,				// Reserved
        NULL,				// a promptstruct
        CRYPTPROTECT_UI_FORBIDDEN,
        &DataOut)){
    
		// The function failed. Report the error.   
		dprintf(D_ALWAYS, "Encryption error! errorcode=%lu \n",
			GetLastError());
    }


	lsaString->Buffer = (PWSTR)DataOut.pbData;
	lsaString->Length = (USHORT)DataOut.cbData;

	dprintf(D_FULLDEBUG, "Attempting to store %d bytes to reg key...\n",
		 lsaString->Length);
	
	// now we can (finally) grab the private data
	ntsResult = LsaStorePrivateData(
				policyHandle, 		/* LSA_HANDLE */
				&keyName,			/* LSA_UNICODE_STRING registry key name */
				lsaString	 		/* LSA_UNICODE_STRING private data */
		);
	
	// be tidy with the silly policy handles
	LsaClose(policyHandle);

	// clean up our encrypted data
	if ( DataOut.pbData != NULL ) {
		LocalFree(DataOut.pbData);
	}

	return (ntsResult == ERROR_SUCCESS);
}


void
lsa_mgr::InitLsaString( PLSA_UNICODE_STRING LsaString,  PCWSTR String ) {
	DWORD StringLength;
	if(String == NULL) {
		LsaString->Buffer = NULL;
		LsaString->Length = 0;
		LsaString->MaximumLength = 0;
		return;
	}
	//StringLength = lstrlenW(String);
	StringLength = wcslen(String);
	LsaString->Buffer = (PWSTR) String;
	LsaString->Length = (USHORT) StringLength * sizeof(WCHAR);
	LsaString->MaximumLength = (USHORT) (StringLength + 1) * sizeof(WCHAR);
}

// the purpose of this method is to guarantee that the data buffer is null terminated
void
lsa_mgr::extractDataString() {
	if ( this->DataBuffer ) { // no op if there's no data
		int strlength = this->DataBuffer->Length/sizeof(WCHAR);
		this->Data_string = new wchar_t[ strlength +1]; //length + null
		wcsncpy( Data_string, DataBuffer->Buffer, strlength );
		Data_string[strlength] = L'\0'; // make sure it's null terminated
	} else {
		dprintf(D_ALWAYS, "lsa_mgr::extractDataString() has been "
			"called with no data\n");
	}
}

LPWSTR
lsa_mgr::findInDataString( const LPWSTR Login, bool case_sensitive ) {
	int look_for_size = wcslen(Login) + 3; // delimiter+delimiter+null
	wchar_t* look_for = new wchar_t[look_for_size];
	
	// we're gonna look though the data string hoping to find this:
	// \nDesired_Login\t
	
	look_for[0] = CC_RECORD_DELIM;
	look_for[1] = L'\0';
	
	wcscat(look_for, Login );
	look_for[wcslen(look_for)+1] = L'\0';
	look_for[wcslen(look_for)] = CC_DATA_DELIM;
		
//	wprintf(L"Looking for '%s'\n", look_for);
	wchar_t* result; 
	if ( case_sensitive ) {
		result = wcsstr(Data_string, look_for);
	} else {
		result = wcsstri(Data_string, look_for);
	}
	delete[] look_for;
	return result;
}

// strstr that's case insensitive
wchar_t* 
lsa_mgr::wcsstri(wchar_t* haystack, wchar_t* needle) {
	wchar_t* h_lwr = NULL; // lowercase versions of
	wchar_t* n_lwr = NULL; // the above args
	wchar_t* match = NULL;

	if ( haystack && needle ) {
		h_lwr = new wchar_t[wcslen(haystack)+1];
		n_lwr = new wchar_t[wcslen(needle)+1];
		
		// make lowercase copies
		wcscpy(h_lwr, haystack);
		wcscpy(n_lwr, needle);
		wcslwr(h_lwr);
		wcslwr(n_lwr);

		// do the strstr
		match = wcsstr(h_lwr, n_lwr);
		if ( match ) {
			// set match to point to original haystack
			// using offset
			match = &haystack[match-h_lwr];
		} else {
			match = NULL;
		}
		
		delete[] h_lwr;
		delete[] n_lwr;
	}

	return match;
}


void 
doAdd() {
	lsa_mgr* foo = new lsa_mgr();
#if 1 // just read directly into wchar buffers
	wchar_t wszLogin[1024];
	wchar_t wszPassw[1024];
	wszLogin[0] = wszPassw[0] = 0;

	printf("Enter Login: ");
	_getws_s(wszLogin, COUNTOF(wszLogin));

	printf("Enter Password: ");
	_getws_s(wszPassw, COUNTOF(wszPassw));

	foo->add( wszLogin, wszPassw );

	SecureZeroMemory(wszPassw, sizeof(wszPassw));
	SecureZeroMemory(wszLogin, sizeof(wszLogin));
#else
	char inBuf[1024];
	wchar_t *Login=NULL, *Passw=NULL;
	
	printf("Enter Login: ");
	gets_s(inBuf, COUNTOF(inBuf));
	Login = foo->charToUnicode( inBuf );
	
	printf("Enter Password: ");
	gets_s(inBuf, COUNTOF(inBuf));
	Passw = foo->charToUnicode( inBuf );

	foo->add( Login, Passw );

	SecureZeroMemory(Passw, sizeof(WCHAR)*wcslen(Passw)+1);
	delete[] Passw;
	Passw = NULL;

	SecureZeroMemory(Login, sizeof(WCHAR)*wcslen(Login)+1);
	delete[] Login;
	Login = NULL;
#endif
	// cleanup 
	delete foo;
}

void doRemove() {
	
	lsa_mgr* foo = new lsa_mgr();
	
	printf("Enter Login: ");
#if 1 // just read directly into wchar buffer
	wchar_t wszLogin[1024];
	_getws_s(wszLogin, COUNTOF(wszLogin));

	foo->remove( wszLogin );

	SecureZeroMemory(wszLogin, sizeof(wszLogin));
#else
	char inBuf[1024];
	wchar_t *Login=NULL;
	gets_s(inBuf);
	Login = foo->charToUnicode( inBuf );
	
	foo->remove( Login );

	ZeroMemory(Login, sizeof(WCHAR)*wcslen(Login)+1);
	delete[] Login;
	Login = NULL;
#endif
	// cleanup 
	delete foo;

}

void doQuery() {
	
	lsa_mgr* foo = new lsa_mgr();
	
#if 1 // just read directly into wchar buffer
	wchar_t wszLogin[1024];
	wszLogin[0] = 0;
	printf("Enter Login: ");
	_getws_s(wszLogin, COUNTOF(wszLogin));

	wchar_t * pszPassw = foo->query( wszLogin );

	printf("Password is %S\n", pszPassw ? pszPassw : L"Not Found");

	// cleanup 

	if ( pszPassw ) {
		SecureZeroMemory(pszPassw, sizeof(WCHAR)*wcslen(pszPassw));
		delete[] pszPassw;
		pszPassw = NULL;
	}

	SecureZeroMemory(wszLogin, sizeof(wszLogin));

#else

	char inBuf[1024];
	wchar_t *Login=NULL, *Passw=NULL;

	printf("Enter Login: ");
	gets_s(inBuf);
	Login = foo->charToUnicode( inBuf );
	
	
	Passw = foo->query( Login );

	printf("Password is %S\n", Passw ? Passw : L"Not Found");

	// cleanup 

	if ( Passw ) {
		ZeroMemory(Passw, sizeof(WCHAR)*wcslen(Passw));
		delete[] Passw;
		Passw = NULL;
	}

	ZeroMemory(Login, sizeof(WCHAR)*wcslen(Login)+1);
	delete[] Login;
	Login = NULL;

#endif

	delete foo;
}

void doPrintAll() {

	lsa_mgr *foo = new lsa_mgr();
	foo->printAllData();
	delete foo;
}

void doClearAll() {
	lsa_mgr *foo = new lsa_mgr();
	foo->purgeAllData();
	delete foo;
}

void printMenu() {

	printf("\n\n1. %s\n2. %s\n3. %s\n4. %s\n5. %s\nExit\n\n> ", 
			"Add new password",
			"Remove a password",
			"Query",
			"Print all passwords",
			"Clear all passwords" );
}

int interactive() {

	char inBuf[256];

	while (1) {
		printMenu();
		gets_s(inBuf, COUNTOF(inBuf));
		switch( inBuf[0] ) {
			case '1' : doAdd(); break;
			case '2' : doRemove(); break;
			case '3' : doQuery(); break;
			case '4' : doPrintAll(); break;
			case '5' : doClearAll(); break;
			default: return 0; break;
		}
	}

}

#endif // WIN32

