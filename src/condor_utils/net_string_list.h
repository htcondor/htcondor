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


#ifndef __CONDOR_NET_STRING_LIST_H
#define __CONDOR_NET_STRING_LIST_H

#include "condor_common.h"
#include "string_list.h"


/*

  This class is derived from StringList, but adds support for
  network-related string manipulations.  This way, we can have
  ClassAds depend on the regular, simple StringList, without pulling
  in dependencies on all our network-related utilities.  This is sort
  of a hack, but seems like a lesser evil, especially since we only
  use this string_withnetwork() method in a single place in the code
  (condor_daemon_core.V6/condor_ipverify.C). 
*/
class NetStringList : public StringList {
public:
	NetStringList(const char *s = NULL, const char *delim = " ," ); 

		// ip: the IP address to find
		// matches: if not NULL, list in which to insert all matching entries
		// returns true if any matches found
		// The following sorts of entries may exist in our list:
		// 192.168.10.1
		// 192.168.*
		// 192.168.0.0/24 
		// 192.168.0.0/255.255.255.0
	bool find_matches_withnetwork( const char *ip, StringList *matches );
};

#endif /* __CONDOR_NET_STRING_LIST_H */

