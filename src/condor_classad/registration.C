//
// registration.C : 
//
//   implementation file for the Registration class.
//
//   Written by Wei Chen & Taxiao Wang, Fall 1995
//

#include <string.h>
#include <assert.h>
#include <except.h>
#include <stdlib.h>	// for realloc
#include "condor_registration.h"
#include "condor_common.h"

static  char *_FileName_ = __FILE__;         // Used by EXCEPT (see except.h)

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
int Registration::RegisterType(char *type)
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





