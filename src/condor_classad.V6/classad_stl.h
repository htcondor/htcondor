/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, CONDOR Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI, and Rajesh Raman.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU Lesser General
 * Public License as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 *
 *********************************************************************/

#ifndef __CLASSAD_STL_H__
#define __CLASSAD_STL_H__

#ifdef __GNUC__
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
	

#ifdef __GNUC__
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
