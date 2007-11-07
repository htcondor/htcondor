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


 

#include <signal.h>
#define N_POSIX_SIGS 19

void display_sigset( char * msg, sigset_t * mask );

/* pointer to function taking one integer arg and returning void */
typedef void (*SIGNAL_HANDLER)(int);

class EventHandler {
public:
	EventHandler( void (*f)(int), sigset_t m );
	void install();
	void de_install();
	void allow_events( sigset_t & );
	void block_events( sigset_t & );
	void display();
private:
	SIGNAL_HANDLER		func;
	sigset_t			mask;
	struct sigaction	o_action[ N_POSIX_SIGS ];
	int  				is_installed;
};


#if 0
class CriticalSection {
public:
	begin();
	end();
	deliver_one();
private:
	setset_t		save_mask;
	int				is_critical;
}
#endif
