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
// class Class contains information about how to classify a job base on its
// requirements.
//

#include "condor_common.h"
#include "condor_classad.h"
#include "condor_classifiedjobs.h"

Class::Class(ExprTree* classDef)
{
    definition = classDef;
    list = NULL;
    next = NULL;
}

Class::~Class()
{
	delete definition;
	delete list;
}

const ExprTree* Class::Definition()
{
	return definition;
}

int Class::ClassRequired(ExprTree* requirement)
{
    if(!requirement)
    {
	return FALSE;
    }

	// All RequireClass code has been yanked out; just return FALSE  --Rajesh
	// return requirement->RequireClass(definition);
	return FALSE;
}

ClassList::~ClassList()
{
    Class*	tmpClass;

    while(classes)
    {
        tmpClass = classes;
        classes = classes->next;
        delete tmpClass;
    }
}

int ClassList::BuildClassList(char* classDefFile, char* contextsFile)
{
	if(!classDefFile)
	{
		return FALSE;
	}

	char        line[1000];
	Class*      tmpClass;
	ExprTree*   def;
	ifstream    iFile;

	iFile.open(classDefFile);
	while(iFile.getline(line, 1000))
	{
		if(Parse(line, def) != 0)
		{
			delete def;
			return FALSE;
		}
		tmpClass = new Class(def);
		if(!classes)
		{
			classes = tmpClass;
			tail = classes;
		}
		else
		{
			tail->next = tmpClass;
			tail = tail->next;
		}
		numOfClasses++;
	}

	if(contextsFile)
	{
	}

	iFile.close();
	return TRUE;
}

int ClassList::ClassRequired(int index, ExprTree* requirement)
{
    if(index >= numOfClasses || index < 0)
    {
	return FALSE;
    }

    return IndexClass(index)->ClassRequired(requirement);
}

int ClassList::ClassIndex(ExprTree* definition)
{
    Class*	tmpClass;
    int		i = 0;		// class index

    for(tmpClass = classes; tmpClass; tmpClass = tmpClass->next)
    {
	if(*(tmpClass->definition) == *definition)
	{
	    return i;
	}
	i++;
    }
    return -1;
}

Class* ClassList::IndexClass(int index)
{
    if(index == 0)
    {
	return classes;
    }

    Class*	tmpClass = classes;

    for( ; index > 0; index--)
    {
	tmpClass = tmpClass->next;
    }
    return tmpClass;
}
