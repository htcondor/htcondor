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

#ifndef _CONDOR_SIG_INSTALL_H
#define _CONDOR_SIG_INSTALL_H

#ifdef __cplusplus 
extern "C" {
#endif

typedef void (*SIG_HANDLER)(int);

void install_sig_handler( int sig, SIG_HANDLER handler );
#ifndef WIN32
void install_sig_handler_with_mask( int sig, sigset_t* set, SIG_HANDLER handler );
#endif
void block_signal( int sig );
void unblock_signal( int sig );

#ifdef __cplusplus 
}
#endif

#endif /* _CONDOR_SIG_INSTALL_H */
