/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
