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

#ifndef _CONDOR_RANDOM_NUM
#define _CONDOR_RANDOM_NUM

#if defined(__cplusplus)
extern "C" {
#endif

int set_seed(int seed);
int get_random_int(void);
unsigned int get_random_uint(void);
float get_random_float(void);
double get_random_double( void );
int timer_fuzz(int period);

#if defined(__cplusplus)
}
#endif

#endif /* _CONDOR_RANDOM_NUM */
