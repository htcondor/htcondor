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

#ifndef WIN32

#ifdef __cplusplus 
extern "C" {
#endif

typedef void (*SIG_HANDLER)(int);
typedef void (*SIG_ACTION)(int, siginfo_t *, void *);

void install_sig_handler( int sig, SIG_HANDLER handler );
void install_sig_handler_with_mask( int sig, sigset_t* set, SIG_HANDLER handler );
void install_sig_action_with_mask( int sig, sigset_t* set, SIG_ACTION handler );
void block_signal( int sig );
void unblock_signal( int sig );

#ifdef __cplusplus 
}
#endif

#endif /* !WIN32 */

#endif /* _CONDOR_SIG_INSTALL_H */
