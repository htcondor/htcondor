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


#include "condor_common.h" 

#include "net_string_list.h"
#include "condor_debug.h"
#include "internet.h"


NetStringList::NetStringList(const char *s, const char *delim ) 
	: StringList(s,delim)
{
		// nothing else to do
}


// string_withnetmask() handles the following four forms:
//
// 192.168.10.1
// 192.168.*
// 192.168.0.0/24 
// 192.168.0.0/255.255.255.0
//
// this only checks against strings which are in the above form.  so, just a
// hostname will not match in this function.
//
// function returns a string pointer to the pattern it matched against.

bool
NetStringList::find_matches_withnetwork(const char *ip_address,StringList *matches)
{
	char *x;
	struct in_addr test_ip, base_ip, mask;
	
	// fill in test_ip
	if (!is_ipaddr(ip_address, &test_ip)) {
		// not even a valid IP
		return false;
	}

	strings.Rewind();
	while ( (x = strings.Next()) ) {
		if (is_valid_network(x, &base_ip, &mask)) {
			// test_ip, base_ip, and mask are all filled

			// logic here:
			// all bits specified in the mask must be equal in the
			// test_ip and base_ip.  so, AND both of them with the mask,
			// and then compare them.
			if ((base_ip.s_addr & mask.s_addr) == (test_ip.s_addr & mask.s_addr)) {
				if( matches ) {
					matches->append( x );
				}
				else {
					return true;
				}
			}
		}
	}

	if( matches ) {
		return !matches->isEmpty();
	}
	return false;
}

