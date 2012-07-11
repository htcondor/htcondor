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

#ifndef ___FUNCTIONAL_H__
#define ___FUNCTIONAL_H__

#include "config.h" 

#ifdef PREFER_CPP11
	#include <functional>
	#define _bind std::bind
	#define _function std::function
#elif defined(PREFER_TR1)
	#include <tr1/functional>
	#define _bind std::tr1::bind
	#define _function std::tr1::function
#else
	#include <boost/bind.hpp>
	#include <boost/function.hpp>
	#define _bind boost::bind
	#define _function boost::function
#endif

#endif
