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

#ifndef JAVA_CONFIG_H
#define JAVA_CONFIG_H

#include "condor_arglist.h"

/*
Extract the java configuration from the local config files.
The name of the java executable gets put in 'cmd', and the necessary
arguments get put in 'args'.  If you have other dirs or jarfiles
that should be placed in the classpath, provide them in 'extra_classpath'.
*/

int java_config( std::string &cmd, ArgList &args, const std::vector<std::string>* extra_classpath );

#endif
