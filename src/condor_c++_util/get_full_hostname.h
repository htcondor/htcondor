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

#ifndef GET_FULL_HOSTNAME_H
#define GET_FULL_HOSTNAME_H

extern char* get_full_hostname( const char*, 
								struct in_addr *sin_addrp = NULL );

extern char* get_full_hostname_from_hostent( struct hostent* host_ptr,
											 const char* host );

#endif /* GET_FULL_HOSTNAME_H */
