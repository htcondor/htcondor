/*
#  File:     resbuffer.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   23 Mar 2004 - Original release
#   31 Mar 2008 - Switched from linked list to single string buffer
#                 Dropped support for persistent (file) buffer
#                 Async mode handled internally (no longer in server.c)
#
#  Description:
#   Mantain the result line buffer
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
*/

#define ALLOC_CHUNKS 32768
#define ASYNC_MODE_OFF 0
#define ASYNC_MODE_ON  1

/* Exported functions */

int init_resbuffer(void);
int push_result(const char* s);
char* get_lines(void);

