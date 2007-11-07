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
// registration.C : 
//
//   implementation file for the Registration class.
//
//   Written by Wei Chen & Taxiao Wang, Fall 1995
//

#include "condor_common.h"
#include "condor_registration.h"
#include "condor_debug.h"

//
// Constructor of the Registration class.
//
Registration::Registration(int size)
{
    regiTableSize = size;
    regiNumber = 0;
    regiTable = new char*[size];
    if(regiTable == NULL)
    {
        EXCEPT("Warning : you ran out of memory -- quitting !");
    }
    for(int i=0; i < size; i++)
    {
        regiTable[i] = NULL;
    }
}

//
// Destructor of Registration class.
//
Registration::~Registration()
{
    for(int i=0; i<regiNumber; i++)
    {
        delete []regiTable[i];
    }
    delete []regiTable;
}

//
// This number fucntion of Registration class registers a (new) type of a
// ClassAd instance. If the type is already registered, its associated type
// number, which is the index number of that type in the registration table,
// is returned. Otherwise, it's registered and an associated type number is
// assigned and returned. 
//
// The type is case-insensitive.
//
int Registration::RegisterType(const char *type)
{
    for(int i=0; i<regiNumber; i++)
    {
        if(!strcasecmp(regiTable[i], type))
        {
            return i;                      // type previously registered.
        }
    }
                                           // this is a new type.
    if(regiNumber >= regiTableSize)
    {                                      // table is full, reallocation
		int i;
        int tempSize = 2 * regiTableSize;  // necessary. 
		char **tmp = NULL;

		/* This code mimics a realloc, but for new'ed memory */

		/* make a new array of the desired size */
		tmp = new char*[tempSize];
        if(tmp == NULL) {
            EXCEPT("Registration::RegisterType(): out of memory!");
        }

		/* copy the known bits over */
        for(i=0; i<regiTableSize; i++) {
			tmp[i] = regiTable[i];
		}

		/* initialize the rest to NULL */
        for(i=regiTableSize; i<tempSize; i++) {
			tmp[i] = NULL;
		}

		/* delete the old array and move the pointer */
		regiTableSize = tempSize;
		delete [] regiTable;
		regiTable = tmp;
    }
  
    regiTable[regiNumber] = new char[strlen(type)+1];
    if(regiTable[regiNumber] == NULL)
    {
		EXCEPT("Registration::RegisterType(): out of memory!");
    }
    strcpy(regiTable[regiNumber], type);
    regiNumber++;
    return (regiNumber-1);                 // new type number.
}





