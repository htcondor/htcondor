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
#ifndef MY_HOSTNAME_H
#define MY_HOSTNAME_H

#if defined( __cplusplus )
extern "C" {
#endif

extern	char*	my_hostname();
extern	char*	my_full_hostname();
extern	unsigned int	my_ip_addr();
extern	struct in_addr*	my_sin_addr();
extern	char*	my_ip_string();  
extern  void	init_full_hostname();
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
