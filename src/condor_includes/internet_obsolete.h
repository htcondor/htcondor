/*******char * ipport_to_string(const unsigned int ip, const unsigned short port)********************************************************
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

/* This file defines some of the obsolete functions from internet.h
 * Except check point server, it should not be included!
 */

#ifndef INTERNET_OBSOLETE_H
#define INTERNET_OBSOLETE_H

#if defined(__cplusplus)
extern "C" {
#endif

char * ipport_to_string(const unsigned int ip, const unsigned short port);
struct sockaddr_in * getSockAddr(int sockfd);

#if defined(__cplusplus)
}
#endif

#endif /* INTERNET_OBSOLETE_H */
