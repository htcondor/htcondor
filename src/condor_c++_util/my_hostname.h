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

#ifndef MY_HOSTNAME_H
#define MY_HOSTNAME_H

#if defined( __cplusplus )
extern "C" {
#endif

extern	char*	my_hostname( void );
extern	char*	my_full_hostname( void );
extern	unsigned int	my_ip_addr( void );
extern	struct in_addr*	my_sin_addr( void );
extern	char*	my_ip_string( void );
extern  void	init_full_hostname( void );
extern  void	init_ipaddr( int config_done );

#if defined( __cplusplus )
}
#endif

#if defined( __cplusplus )
#include "stream.h"

// If the specified attribute name is recognized as an attribute used
// to publish a daemon IP address, this function replaces any
// reference to the default host IP with the actual connection IP in
// the attribute's expression string.  If no replacement happens,
// new_expr_string will be NULL.  Otherwise, it will be a new buffer
// allocated with malloc().  The caller should free it.

// You might consider this a dirty hack (and it is), but of the
// methods that were considered, this was the one with the lowest
// maintainance, least overhead, and least likelihood to have
// unintended side-effects.

void ConvertDefaultIPToSocketIP(char const *attr_name,char const *old_expr_string,char **new_expr_string,Stream& s);

// This is a convenient interface to ConvertDefaultIPToSocketIP().
// If a replacement occurs, expr_string will be freed and replaced
// with a new buffer allocated with malloc().  The caller should free it.

void ConvertDefaultIPToSocketIP(char const *attr_name,char **expr_string,Stream& s);

#endif

#endif /* MY_HOSTNAME_H */
