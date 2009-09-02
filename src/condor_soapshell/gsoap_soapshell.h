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

//gsoap condor service name: condorSoapshell

#import "gsoap_daemon_core_types.h"

#import "gsoap_daemon_core.h"

int condor__runCommandWithClassAd(struct condor__ClassAdStruct ad,
		struct condor__runCommandWithClassAdResponse {
			struct condor__ClassAdStructAndStatus response;
		} & result);


int condor__runCommandWithString(char *adSerializedToString, 
		struct condor__runCommandWithStringResponse {
			struct condor__StringAndStatus response;
		} & result );
