/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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



