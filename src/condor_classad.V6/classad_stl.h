/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

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
  #if (__GNUC__ == 3 && __GNUC_MINOR__ > 0)
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
