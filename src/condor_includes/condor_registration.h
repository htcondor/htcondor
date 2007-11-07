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

//
// registration.h :
//
//   registration of the types of ClassAd instances. 
//
//   Written by Wei Chen & Taxiao Wang, Fall 1995.
//

#ifndef _REGISTRATION_H
#define _REGISTRATION_H

class Registration
{ 
    public:

        Registration(int = 10);           // constructor.
        ~Registration();                  // destructor.
        int RegisterType(const char *);   // it registers a (new) type. 

    private:

        char **regiTable;                 // registration table.
        int regiTableSize;                // size of the registration table. 
        int regiNumber;                   // number of registrations.
};

#endif
