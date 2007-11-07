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

#ifndef _CKPT_MODE
#define _CKPT_MODE

#if defined(__cplusplus)
extern "C" {
#endif

#define CKPT_MODE_USE_COMPRESSION	(1<<0)	/* write a compressed ckpt */
#define CKPT_MODE_ABORT				(1<<1)	/* don't write a ckpt at all */
#define CKPT_MODE_SLOW				(1<<2)	/* checkpoint slowly */
#define CKPT_MODE_MSYNC				(1<<3)	/* msync all dirty pages */

#if defined(__cplusplus)
}
#endif

#endif /* _CKPT_MODE */


