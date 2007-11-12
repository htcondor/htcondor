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

/*
Chirp C Client
*/

#ifndef CHIRP_PROTOCOL_H
#define CHIRP_PROTOCOL_H

#define CHIRP_LINE_MAX 1024
#define CHIRP_VERSION 2

#define CHIRP_ERROR_NOT_AUTHENTICATED -1
#define CHIRP_ERROR_NOT_AUTHORIZED -2
#define CHIRP_ERROR_DOESNT_EXIST -3
#define CHIRP_ERROR_ALREADY_EXISTS -4
#define CHIRP_ERROR_TOO_BIG -5
#define CHIRP_ERROR_NO_SPACE -6
#define CHIRP_ERROR_NO_MEMORY -7
#define CHIRP_ERROR_INVALID_REQUEST -8
#define CHIRP_ERROR_TOO_MANY_OPEN -9
#define CHIRP_ERROR_BUSY -10
#define CHIRP_ERROR_TRY_AGAIN -11
#define CHIRP_ERROR_UNKNOWN -127

#endif

