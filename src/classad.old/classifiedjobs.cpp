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
// class Class contains information about how to classify a job base on its
// requirements.
//

#define _CONDOR_ALLOW_OPEN 1	// because fstream interface defines open
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

/*
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
*/

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
