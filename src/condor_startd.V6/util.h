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

#ifndef _UTIL_H
#define _UTIL_H

#include "condor_common.h"
#include "condor_classad.h"

class Stream;
class Resource;
class StringList;

// Our utilities 
void	cleanup_execute_dir(int pid, char const *exec_path);
void	cleanup_execute_dirs( StringList &list );
void	check_execute_dir_perms( StringList &list );
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
