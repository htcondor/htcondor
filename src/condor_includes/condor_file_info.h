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
