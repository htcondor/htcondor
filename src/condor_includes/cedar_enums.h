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

#ifndef CEDAR_ENUMS_H
#define CEDAR_ENUMS_H

/* Special enums we use inorder to have nice overloads of the cedar code()
	function. It is very important that we have these assigned to intmax like
	you see here. This makes the enum able to be a 32-bit quantity.
	Scary, isn't it? */
enum open_flags_t { __open_flags_t_dummy_value = INT_MAX };
enum condor_errno_t { __errno_t_dummy_value = INT_MAX };
enum condor_mode_t { __mode_t_dummy_value = INT_MAX };

#endif
