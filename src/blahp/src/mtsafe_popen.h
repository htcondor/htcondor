/*
#  File:     mtsafe_popen.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
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

/* Function prototypes */

FILE * mtsafe_popen(const char *command, const char *type);
int mtsafe_pclose(FILE *stream);

/* Thread safe command execution and output capture */
int exe_getout(char *const command, char *const environment[], char **cmd_output);
int exe_getouterr(char *const command, char *const environment[], char **cmd_output, char **cmd_error);
/* command:     the command to execute
   environment: extra environment to be set before executing <command>
                in addition to the current environment
   cmd_output:  dinamically allocated buffer contaning the standard output
   cmd_error:   dinamically allocated buffer contaning the standard error
   With exe_getout the standard error is printed on stderr, preceded by <command> itself
*/

