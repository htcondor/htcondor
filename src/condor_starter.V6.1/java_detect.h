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


#ifndef _CONDOR_JAVA_DETECT_H
#define _CONDOR_JAVA_DETECT_H

#include "condor_classad.h"

/*
Probe the Java configuration and return a ClassAd describing
its properties.  If the ad is empty or zero, then Java is
not available here.  Note that the attribute names present
here are decided on by the JVM, not Condor, so some translation
may be necessary to match the Condor schema.
*/

ClassAd * java_detect();

#endif
