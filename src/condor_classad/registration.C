/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
        int tempSize = 2 * regiTableSize;  // necessary. 
        regiTable = (char **) realloc(regiTable, tempSize*sizeof(char *));
        if(regiTable == NULL)
        {
            EXCEPT("Warning : you ran out of memory -- quitting !");
        }
        for(int i=regiTableSize; i<tempSize; i++)
	{
	    regiTable[i] = NULL;
	}
	regiTableSize = tempSize;
    }
  
    regiTable[regiNumber] = new char[strlen(type)+1];
    if(regiTable[regiNumber] == NULL)
    {
        EXCEPT("Warning : you ran out of memory -- quitting !");
    }
    strcpy(regiTable[regiNumber], type);
    regiNumber++;
    return (regiNumber-1);                 // new type number.
}





