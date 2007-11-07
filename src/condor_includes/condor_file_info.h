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

#ifndef _FILE_INFO
#define _FILE_INFO

/*
  Methods to access a file.  These are given to the Condor user library
  code by the shadow in response to the CONDOR_file_info and
  CONDOR_std_file_info RPCs.
*/
  
enum {
	IS_FILE,		/* Open as ordinary file */
	IS_PRE_OPEN,	/* File is already open at fd number N (pipe) */
	IS_AFS,			/* File is accessible via AFS */
	IS_NFS,			/* File is accessible via NFS */
	IS_RSC,			/* Open the file via remote system calls */
	IS_IOSERVER,	/* Open the file using the IO Server */
	IS_LOCAL		/* Open the file on the local file system
					   (/dev/null for example) */
};


#endif
