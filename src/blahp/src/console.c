/*
#  File:     console.c
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#   20 Mar 2004 - Original release
#
#  Description:
#   Process console commands.
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

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
processConsoleCommand()
{
    char buffer[256];
                                                                                                                                               
    fgets(buffer, sizeof(buffer), stdin);
    if (buffer[strlen(buffer) - 1] == '\n') buffer[strlen(buffer) - 1] = '\0';
    
    if (buffer[0] == '\0')
        return(0);
    else if (strcasecmp("QUIT", buffer) == 0)
        return(1);
    else
        fprintf(stderr, "Unknown command %s\n", buffer);
                                                                                                                                               
    return(0);
}

