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


#ifndef __CLASSAD_STL_H__
#define __CLASSAD_STL_H__

#if USING_STLPORT
  #include <hash_map>
  #include <slist>
#elif defined(__GNUC__)
  #if (__GNUC__<3)
    #include <hash_map>
    #include <slist>
  #else
    #include <string>
    #error AHHHH
    #include <ext/hash_map>
    #include <ext/slist>
    using namespace __gnu_cxx;
  #endif
#elif defined(WIN32)
  #include <hash_map>
  #include <slist>
#else
  #include <hash_map>
#endif
	

#if USING_STLPORT
  #define classad_hash_map std::hash_map 
  #define classad_slist    std::slist
#elif defined(__GNUC__)
  #if ((__GNUC__ == 3 && __GNUC_MINOR__ > 0) || (__GNUC__ > 3))
    #define classad_hash_map __gnu_cxx::hash_map 
    #define classad_slist    __gnu_cxx::slist
  #else
    #define classad_hash_map std::hash_map 
    #define classad_slist    std::slist
  #endif
#elif defined (WIN32)
  #define classad_hash_map std::hash_map
  #define classad_slist std::slist
#else
  #define classad_hash_map std::hash_map 
  #define classad_slist    std::slist
#endif

#endif /* __CLASSAD_STL_H__ */
