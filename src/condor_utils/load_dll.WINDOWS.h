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

#ifndef _LOAD_DLL_
#define _LOAD_DLL_

/***************************************************************
 * Silly macros to make is obvious if a variable is used as 
 * INput or as OUTput.
 ***************************************************************/

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

/***************************************************************
 * LoadDLL class
 ***************************************************************/

class LoadDLL {

public:
	
	LoadDLL () throw ();
    LoadDLL (LPCSTR name ) throw ();
	virtual ~LoadDLL () throw (); 
	
	bool load ( LPCSTR name );
	void unload ();
	bool loaded () const;

	/*
    // not valid in VC6
    template<class R>
	R getProcAddress ( LPCSTR name );
    */

    FARPROC getProcAddress ( LPCSTR name );

private:
	
	HMODULE _dll;

};

/*
// not valid in VC6
template<class R>
R 
LoadDLL::getProcAddress ( LPCSTR name ) {

	if ( NULL != _dll ) {
		return (R) GetProcAddress ( _dll, name );
	}

	return (R) NULL;

}
*/

#endif // _LOAD_DLL_
