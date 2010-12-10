/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"

#include "string_conversion.WINDOWS.h"

/* convert an ANSI string to Unicode */
LPWSTR 
ProduceWFromA(PCSTR as) {
	if ( !as ) { return NULL; }
	size_t n = strlen ( as ) + 1;
	PWSTR ws = new WCHAR[n];
	ASSERT ( ws );
	*ws	= NULL;
	MultiByteToWideChar ( CP_ACP, 0, as, -1, ws, n );
	return ws;
}

/* convert a Unicode string to ANSI */
LPSTR 
ProduceAFromW(PCWSTR ws) {
	if ( !ws ) { return NULL; }
	size_t n = wcslen ( ws ) + 1;
	PSTR as = new CHAR[n];
	ASSERT ( as );
	*as	= NULL;
	WideCharToMultiByte ( CP_ACP, 0, ws, -1, as, n, NULL, NULL );
	return as;
}

/*
PUNICODE_STRING
ProduceUSFromW ( PCWSTR source ) {
	UNICODE_STRING *us	= new UNICODE_STRING;
	assert ( us );
	UINT n				= ( NULL == source ? 0 : wcslen ( source ) );
	us->Length			= n * sizeof ( WCHAR );
	us->MaximumLength	= (n + 1) * sizeof ( WCHAR );
	us->Buffer			= wcsdup ( source );
	return us;
}

PLSA_UNICODE_STRING 
ProduceLSAUSFromW ( PCWSTR source ) {
	LSA_UNICODE_STRING *us = new LSA_UNICODE_STRING;
	assert ( us );
	UINT n				= ( NULL == source ? 0 : wcslen ( source ) );
	us->Length			= n * sizeof ( WCHAR );
	us->MaximumLength	= (n + 1) * sizeof ( WCHAR );
	us->Buffer			= wcsdup ( source );
	return us;
}
*/
