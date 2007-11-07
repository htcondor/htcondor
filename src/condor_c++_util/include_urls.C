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


 

#include "url_condor.h"



extern void init_cbstp();
extern void init_file();
extern void init_http();
extern void init_filter();
extern void init_mailto();
extern void init_cfilter();
extern void init_ftp();

void	(*protocols[])() = {
	init_cbstp,
	init_file,
	init_http,
	init_filter,
	init_mailto,
	init_cfilter,
	init_ftp,
	0
	};
