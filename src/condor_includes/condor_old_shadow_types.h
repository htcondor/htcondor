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


 
#ifndef _OLD_SHADOW_TYPES_H_
#define _OLD_SHADOW_TYPES_H_

typedef struct {
	int		port1;
	int		port2;
} PORTS;


typedef struct {        /* record sent by startd to shadow */
	int		version_num;/* always negative */
	PORTS	ports;
	int     ip_addr;    /* internet addressof executing machine */
	char*   server_name;/* name of executing machine */
} StartdRec;
	/* Startd version numbers : always negative  */

#define VERSION_FOR_FLOCK   -1

#endif
