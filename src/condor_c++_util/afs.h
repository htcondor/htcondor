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

 

/*
  Use AFS user level commands to get information about things like the
  AFS cell of the current workstation, or the AFS cell, volume, etc.
  of given files.
*/

#ifndef CONDOR_AFS_H
#define CONDOR_AFS_H

#include <limits.h>
#include <stdio.h>

#if defined(__cplusplus)

class AFS_Info {
public:
	AFS_Info();
	~AFS_Info();
	void display();
	char *my_cell();
	char *which_cell( const char *path );
	char *which_vol( const char *path );
	char *which_srvr( const char *path );
private:
	char *find_my_cell();
	char *parse_cmd_output(
			FILE *fp, const char *pat, const char *left, const char *right );

	int has_afs;
	char fs_pathname[ _POSIX_PATH_MAX ];
	char vos_pathname[ _POSIX_PATH_MAX ];
	char *my_cell_name;
};

#endif /* __cplusplus */

/*
  Simple C interface.
*/
#if defined(__cplusplus)
extern "C" {
#endif

char *get_host_cell();
char *get_file_cell( const char *path );
char *get_file_vol( const char *path );
char *get_file_srvr( const char *path );

#if defined(__cplusplus)
}
#endif

#endif CONDOR_AFS_H
