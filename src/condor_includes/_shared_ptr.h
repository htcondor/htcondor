/***************************************************************
 *
 * Copyright 2012 Red Hat, Inc. 
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

#ifndef ___SHARED_PTR_H__
#define ___SHARED_PTR_H__

#include "config.h" 

#ifdef PREFER_CPP11
	#include <memory>
	#define _weak_ptr std::weak_ptr
	#define _shared_ptr std::shared_ptr
#elif defined(PREFER_TR1)
	#include <tr1/memory>
	#define _weak_ptr std::tr1::weak_ptr
	#define _shared_ptr std::tr1::shared_ptr 
#else
	#include <boost/shared_ptr.hpp>
	#include <boost/weak_ptr.hpp>
	#define _weak_ptr boost::weak_ptr
	#define _shared_ptr boost::shared_ptr
#endif

#endif
