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
#ifndef _UTIL_H
#define _UTIL_H

class ClassAd;
class Stream;
class Resource;

// Our utilities 
void	cleanup_execute_dir(int pid);
void	check_perms(void);
float	compute_rank( ClassAd*, ClassAd* );
int 	create_port( ReliSock* );
bool	reply( Stream*, int );
bool	refuse( Stream* );
bool	caInsert( ClassAd* target, ClassAd* source, const char* attr,
				  const char* prefix = NULL );
bool	configInsert( ClassAd* ad, const char* attr, bool is_fatal );
bool	configInsert( ClassAd* ad, const char* param_name, 
					  const char* attr, bool is_fatal );
Resource* stream_to_rip( Stream* );

VacateType getVacateType( ClassAd* ad );

#endif /* _UTIL_H */
