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

/** strcmp compares two strings the same as strcmp, but considers 
 *  the character 'until' to be the end of string.
 *  @param s1 first string in comparison.
 *  @param s2 second string in comparison.
 *  @param until the character to stop comparing at.
 *  This can be useful for comparing sinful strings, but stopping
 *  at the port number (i.e. just the address part).
 */
int
strcmp_until( const char *s1, const char *s2, const char until ) {
   while ( ( *s1 || *s2 ) && *s1 != until && *s2 != until ) {

      if ( *s1 != *s2 ) {
         return( *s2 - *s1 );
      }
      s1++;
      s2++;
   }

	if (*s1 == until && *s2 != until)
	{
		return -1;
	}

	if (*s1 != until && *s2 == until)
	{
		return 1;
	}

   return( 0 );
}



