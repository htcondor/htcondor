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


 

#ifndef MY_USERNAME_H
#define MY_USERNAME_H

/* Returns malloc()ed memory.  Be sure to free() it.  Return NULL on failure */
char *my_username();
/* Returns malloc()ed memory.  Be sure to free() it.  Return NULL on failure */
char *my_domainname( void );

#ifdef WIN32
HANDLE my_usertoken(void);
#endif

#endif
