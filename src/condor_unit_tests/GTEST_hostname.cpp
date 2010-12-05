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
#include <gtest/gtest.h>
#include <netinet/in.h>
#include "ipv6_hostname.h"

using namespace std;

TEST(ipv6, hostname) {
	ipaddr local = get_local_ipaddr();
	printf("ipaddr sinful= %s\n", local.to_sinful().Value());
	MyString hostname = get_local_hostname();
	MyString fqdn = get_local_fqdn();
	printf("hostname= %s\n", hostname.Value());
	printf("fqdn= %s\n", fqdn.Value());
	printf("getting hostname for %s\n", local.to_sinful().Value());
	printf("result = %s\n", get_hostname(local).Value());
	printf("getting hostname with aliases for %s\n", local.to_sinful().Value());
	vector<MyString> hostnames = get_hostname_with_alias(local);
	for (int i = 0; i < hostnames.size(); ++i) {
		printf("%s\n", hostnames[i].Value());
	}
}
