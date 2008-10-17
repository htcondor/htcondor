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