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


#include <gtest/gtest.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "MyString.h"
#include "condor_ipaddr.h"

TEST(ipaddr, from_sinful) {
	ipaddr s;
	ASSERT_TRUE(s.from_sinful("<127.0.0.1:1234>"));
	sockaddr_in sin = s.to_sin();
	in_addr inaddr = sin.sin_addr;
	in_addr inaddr_comp;
	EXPECT_EQ(sin.sin_port, htons(1234));
	inet_pton(AF_INET, "127.0.0.1", &inaddr_comp);
	EXPECT_EQ(inaddr.s_addr, inaddr_comp.s_addr);
}
