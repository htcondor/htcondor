/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
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
