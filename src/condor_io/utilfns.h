/***************************************************************
 *
 * Copyright (C) 1990-2009 Red Hat, Inc.
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

#ifndef __CEDAR_UTIL_FNS__
#define __CEDAR_UTIL_FNS__

extern "C" {

int errno_num_encode( int errno_num );
int errno_num_decode( int errno_num );

int open_flags_encode(int old_flags);
int open_flags_decode(int old_flags);

}

#endif
