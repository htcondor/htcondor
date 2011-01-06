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

// This file is linked into programs that do not link with other condor
// libraries, so don't include the usual condor headers.

#include "config.h"
#include "condor_unsetenv.h"
#include <string.h>

#ifndef HAVE_UNSETENV

/* swiped right out of the bsd libc */
static char *
__findenv(const char *name, int *offset)
{
        extern char **environ;
        register int len;
        register const char *np;
        register char **p, *c;

        if (name == NULL || environ == NULL)
                return (NULL);
        for (np = name; *np && *np != '='; ++np)
                continue;
        len = np - name;
        for (p = environ; (c = *p) != NULL; ++p)
                if (strncmp(c, name, len) == 0 && c[len] == '=') {
                        *offset = p - environ;
                        return (c + len + 1);
                }
        return (NULL);
}

void
unsetenv(const char *name)
{
        extern char **environ;
        register char **p;
        int offset;

        while (__findenv(name, &offset))        /* if set multiple times */
                for (p = &environ[offset];; ++p)
                        if (!(*p = *(p + 1)))
                                break;
}
#endif
