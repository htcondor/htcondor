//******************************************************************************
// Cai, Weiru
//
// class Class contains information about how to classify a job base on its
// requirements.
//
//******************************************************************************

#include <std.h>
#include "classad.h"
#include "classifiedjobs.h"
#include "parser.h"

Class::Class(ExprTree* classDef)
{
    definition = classDef;
    list = NULL;
    next = NULL;
}

int Class::ClassRequired(ExprTree* requirement)
{
    if(!requirement)
    {
	return FALSE;
    }
    return requirement->RequireClass(definition);
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
