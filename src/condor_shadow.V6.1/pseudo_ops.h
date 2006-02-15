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
#if !defined(_PSEUDO_OPS_H)
#define _PSEUDO_OPS_H

int pseudo_register_machine_info(char *uiddomain, char *fsdomain, 
								 char *starterAddr, char *full_hostname);
int pseudo_register_starter_info( ClassAd* ad );
int pseudo_get_job_info(ClassAd *&ad);
int pseudo_get_user_info(ClassAd *&ad);
int pseudo_job_exit(int status, int reason, ClassAd* ad);
int pseudo_register_mpi_master_info( ClassAd* ad );
int pseudo_begin_execution( void );
int pseudo_get_file_info_new( const char *path, char *url );
int pseudo_get_buffer_info( int *bytes_out, int *block_size_out, int *prefetch_bytes_out );
int pseudo_ulog( ClassAd *ad );
int pseudo_get_job_attr( const char *name, char *expr );
int pseudo_set_job_attr( const char *name, const char *expr );
int pseudo_constrain( const char *expr );

#endif
