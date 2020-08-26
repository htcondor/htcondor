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


#ifndef __CLASSAD_CLASSAD_CONTAINERS_H__
#define __CLASSAD_CLASSAD_CONTAINERS_H__

#include <map>
#include <list>

#include <memory>
#include <unordered_map>
	
// Should these be typedefs?
#define classad_unordered std::unordered_map
#define classad_weak_ptr std::weak_ptr
#define classad_shared_ptr std::shared_ptr

#define classad_map   std::map 
#define classad_slist std::list

// G++ uses a shared copy-on-write (COW) std::string on older versions
// classads will force a copy from the cache for COW strings, but
// newer gccs and all other compilers use the small string optimization
// for their std::strings, and this copy just slows things down

#if defined(__GNUC__) && (__GNUC__ < 5)
#define HAVE_COW_STRING
#else
#undef HAVE_COW_STRING
#endif

#endif /* __CLASSAD_CLASSAD_CONTAINERS_H__ */
