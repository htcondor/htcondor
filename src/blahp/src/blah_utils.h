/*
#  File:     blah_utils.h
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   30 Mar 2009 - Original release.
#
#  Description:
#   Utility functions for blah protocol
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

#ifndef BLAHP_UTILS_INCLUDED
#define BLAHP_UTILS_INCLUDED

extern const char *blah_omem_msg;

char *make_message(const char *fmt, ...);
char *escape_spaces(const char *str);

#define BLAH_DYN_ALLOCATED(escstr) ((escstr) != blah_omem_msg && (escstr) != NULL)

#endif /* ifndef BLAHP_UTILS_INCLUDED */
