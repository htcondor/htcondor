/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#ifndef THREAD_CONTROL_H
#define THREAD_CONTROL_H

#include <pthread.h>

	/* We use a big mutex to make certain only one thread is running at a time,
	 * except when that thread would be blocked on I/O or a signal.  We do
	 * this becase many data structures and methods used from utility libraries
	 * are not inheriently thread safe.  So we error on the side of correctness,
	 * and this isn't a big deal since we are only really concerned with the gahp
	 * blocking when we do network communication and REST processing.
	 */

extern pthread_mutex_t global_big_mutex;

#define arc_gahp_grab_big_mutex()	\
	pthread_mutex_lock(&global_big_mutex)
#define arc_gahp_release_big_mutex()	\
	pthread_mutex_unlock(&global_big_mutex)

#endif
