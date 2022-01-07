/*
#  File:     env_helper.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#    10 Mar 2009 - Original release
#
#  Description:
#    Helper functions to manage an execution environment.
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

#ifndef ENV_HELPER_H_INCLUDED
#define ENV_HELPER_H_INCLUDED

typedef char ** env_t;

int push_env(env_t *my_env, const char *new_env);
int copy_env(env_t *rc_dest, const env_t env_src);
int append_env(env_t *rc_dest, const env_t env_src);
void free_env(env_t *my_env);

#endif /*ENV_HELPER_H_INCLUDED*/ 
