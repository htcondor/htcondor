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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "classad/classad_distribution.h"

#include <iostream>

using namespace std;
using namespace classad;

int main(int, char **)
{
    string classad_version;

    ClassAdLibraryVersion(classad_version);

    cout << "ClassAd Library v" << classad_version << "\n";

    /* ----- Namespace support ----- */
    cout << "  Classad namespace:               ";
    cout << "yes\n";

    /* ----- Shared library function support ----- */
    cout << "  Shared library function support: ";
#if defined(UNIX)
    cout << "yes\n";
#else
    cout << "no\n";
#endif

    /* ----- Regular expressions ----- */
    cout << "  Regular expression function:     ";
    cout << "PCRE\n";

    /* ----- GNU version, if applicable ----- */
#if defined __GNUC__ && defined __GNUC_MINOR__
    cout << "  GCC version:                     ";
    cout << __GNUC__ << "." << __GNUC_MINOR__;
#if defined __GNUC_PATCHLEVEL__
    cout << "." << __GNUC_PATCHLEVEL__;
#endif
    cout << endl;
#endif

    return 0;
}
