/*********************************************************************
 *
 * Condor ClassAd library
 * Copyright (C) 1990-2003, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI and Rajesh Raman.
 *
 * This source code is covered by the Condor Public License, which can
 * be found in the accompanying LICENSE file, or online at
 * www.condorproject.org.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
 * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
 * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
 * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
 * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
 * RIGHT.
 *
 *********************************************************************/

#if defined(CLASSAD_DISTRIBUTION)
#include "classad_distribution.h"
#else
#include "condor_classad.h"
#endif

using namespace std;

int main(int argc, char **argv)
{
    string classad_version;

    ClassAdLibraryVersion(classad_version);

    cout << "ClassAd Library v" << classad_version << "\n";

    /* ----- Namespace support ----- */
    cout << "  Classad namespace:               ";
#if defined WANT_NAMESPACES
    cout << "yes\n";
#else
    cout << "no\n";
#endif

    /* ----- Shared library function support ----- */
    cout << "  Shared library function support: ";
#if defined ENABLE_SHARED_LIBRARY_FUNCTIONS
    cout << "yes\n;"
#else
    cout << "no\n";
#endif

    /* ----- Regular expressions ----- */
    cout << "  Regular expression function:     ";
#if defined USE_POSIX_REGEX 
    cout << "POSIX\n";
#elif defined USE_PCRE
    cout << "PCRE\n";
#else
    cout << "Disabled\n";
#endif

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
