/***************************************************************
 *
 * Copyright (C) 2011, Condor Team, Computer Sciences Department,
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
#ifdef UNICODE
#define _TokenizeString _TokenizeStringW
#define _ParseToken _ParseTokenW
#else
#define TokenizeString _TokenizeStringA
#define ParseToken _ParseTokenA
#endif

// tokenize.cpp
LPCWSTR * _TokenizeStringW (LPCWSTR pszInput, int* pcArgs);
LPCSTR  * _TokenizeStringA (LPCSTR  pszInput, int* pcArgs);

// from parsetok.cpp
LPWSTR _ParseTokenW (LPWSTR pszToken);
LPSTR _ParseTokenA (LPSTR pszToken);
