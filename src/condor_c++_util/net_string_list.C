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

#include "condor_common.h" 

#include "net_string_list.h"
#include "condor_debug.h"
#include "internet.h"


NetStringList::NetStringList(const char *s, const char *delim ) 
	: StringList(s,delim)
{
		// nothing else to do
}


// contains_withnetmask() is just like the contains() method except that
// list members can be IP addresses with netmasks in them, and anything on
// the subnet is a match.  two forms are allowed:
//
// 192.168.0.0/24 
// 192.168.0.0/255.255.255.0
//
// this only checks against strings which are in netmask form.  so, just an
// IP address or hostname will not match in this function.
//
// function returns a string pointer to the pattern it matched against.

const char *
NetStringList::string_withnetwork(const char *ip_address)
{
	char *x;
	struct in_addr test_ip, base_ip, mask;
	
	// fill in test_ip
	if (!is_ipaddr(ip_address, &test_ip)) {
		// not even a valid IP
		return NULL;
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
				return x;
			}
		}
	}

	return NULL;
}

