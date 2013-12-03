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

// use get_local_hostname() instead
extern	const char*	my_hostname( void );

// use get_local_fqdn() instead
extern	const char*	my_full_hostname( void );

// use get_local_ipaddr().to_ip_string() instead
extern	const char*	my_ip_string( void );

#include "stream.h"
#include <string>
#include <set>

void init_network_interfaces(int config_done);

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

void ConfigConvertDefaultIPToSocketIP();

// This is a convenient interface to ConvertDefaultIPToSocketIP().
// If a replacement occurs, expr_string will be freed and replaced
// with a new buffer allocated with malloc().  The caller should free it.

void ConvertDefaultIPToSocketIP(char const *attr_name,char **expr_string,Stream& s);

// This interface to ConvertDefaultIPToSocketIP() takes a std::string
// and modifies its contents.
void ConvertDefaultIPToSocketIP(char const *attr_name,std::string &expr_string,Stream& s);

bool network_interface_to_ip(
	char const *interface_param_name,
	char const *interface_pattern,
	std::string &ip,
	std::set< std::string > *network_interface_ips = NULL);

#endif /* MY_HOSTNAME_H */
