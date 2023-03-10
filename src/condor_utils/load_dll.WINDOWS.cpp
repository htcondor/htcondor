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

/***************************************************************
 * Headers
 ***************************************************************/

#include "condor_common.h"
#include "load_dll.WINDOWS.h"

/***************************************************************
 * LoadDLL class
 ***************************************************************/

LoadDLL::LoadDLL () noexcept
: _dll ( NULL ) {
}

LoadDLL::LoadDLL ( LPCSTR name ) noexcept
: _dll ( NULL ) {
    load ( name );
}

LoadDLL::~LoadDLL () noexcept {
	if ( NULL != _dll ) {
		unload ();
	}
}

bool 
LoadDLL::load ( LPCSTR name ) {
	_dll = LoadLibrary ( name );
	if ( NULL == _dll ) {
		return false;
	}
	return true;
}

bool 
LoadDLL::loaded () const {
	return ( NULL != _dll );
}

void 
LoadDLL::unload () {
	if ( NULL != _dll ) {
		FreeLibrary ( _dll );
	}
}

FARPROC 
LoadDLL::getProcAddress ( LPCSTR name ) {
    return GetProcAddress ( _dll, name );
}
