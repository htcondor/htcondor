/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_common.h"
#include "generic_query.h"
#include "condor_attributes.h"
//#include "condor_parser.h"

static char *new_strdup (const char *);

GenericQuery::
GenericQuery ()
{
	// initialize category counts
	integerThreshold = 0;
	stringThreshold = 0;
	floatThreshold = 0;
	
	// initialize pointers
	integerConstraints = 0;
	floatConstraints = 0;
	stringConstraints = 0;
}


GenericQuery::
GenericQuery (const GenericQuery &gq)
{
	copyQueryObject ((GenericQuery &)gq);
}


GenericQuery::
~GenericQuery ()
{
	clearQueryObject ();
	
	// release memory
	if (stringConstraints) delete [] stringConstraints;
	if (floatConstraints)  delete [] floatConstraints;
	if (integerConstraints)delete [] integerConstraints;
}


int GenericQuery::
setNumIntegerCats (const int numCats)
{
	integerThreshold = (numCats > 0) ? numCats : 0;
	if (integerThreshold)
	{
		integerConstraints = new SimpleList<int> [integerThreshold];
		if (!integerConstraints)
			return Q_MEMORY_ERROR;
		return Q_OK;
	}
	return Q_INVALID_CATEGORY;
}


int GenericQuery::
setNumStringCats (const int numCats)
{
	stringThreshold = (numCats > 0) ? numCats : 0;
	if (stringThreshold)
	{
		stringConstraints = new List<char> [stringThreshold];
		if (!stringConstraints)
			return Q_MEMORY_ERROR;
		return Q_OK;
	}
	return Q_INVALID_CATEGORY;
}


int GenericQuery::
setNumFloatCats (const int numCats)
{
	floatThreshold = (numCats > 0) ? numCats : 0;
	if (floatThreshold)
	{	
		floatConstraints = new SimpleList<float> [floatThreshold];
		if (!floatConstraints)
			return Q_MEMORY_ERROR;
		return Q_OK;
	}
	return Q_INVALID_CATEGORY;
}


// add an integer constraint
int GenericQuery::
addInteger (const int cat, int value)
{
    if (cat >= 0 && cat < integerThreshold)
    {
        if (!integerConstraints [cat].Append (value))
            return Q_MEMORY_ERROR;
        return Q_OK;
    }

    return Q_INVALID_CATEGORY;
}

int GenericQuery::
addFloat (const int cat, float value)
{
    if (cat >= 0 && cat < floatThreshold)
    {
        if (!floatConstraints [cat].Append (value))
            return Q_MEMORY_ERROR;
        return Q_OK;
    }

    return Q_INVALID_CATEGORY;
}

int GenericQuery::
addString (const int cat, char *value)
{
    char *x;

    if (cat >= 0 && cat < stringThreshold)
    {
        x = new_strdup (value);
        if (!x) return Q_MEMORY_ERROR;
        stringConstraints [cat].Append (x);
        return Q_OK;
    }

    return Q_INVALID_CATEGORY;
}

int GenericQuery::
addCustomOR (char *value)
{

    char *x = new_strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customORConstraints.Append (x);
	return Q_OK;
}

int GenericQuery::
addCustomAND (char *value)
{
    char *x = new_strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customANDConstraints.Append (x);
	return Q_OK;
}


// clear functions
int GenericQuery::
clearInteger (const int cat)
{
	if (cat >= 0 && cat < integerThreshold)
	{
		clearIntegerCategory (integerConstraints [cat]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}

int GenericQuery::
clearString (const int cat)
{
	if (cat >= 0 && cat < stringThreshold)
	{
		clearStringCategory (stringConstraints [cat]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}

int GenericQuery::
clearFloat (const int cat)
{
	if (cat >= 0 && cat < floatThreshold)
	{
		clearFloatCategory (floatConstraints [cat]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}

int GenericQuery::
clearCustomOR ()
{
	clearStringCategory (customORConstraints);
	return Q_OK;
}

int GenericQuery::
clearCustomAND ()
{
	clearStringCategory (customANDConstraints);
	return Q_OK;
}


// set keyword lists
void GenericQuery::
setIntegerKwList (char **value)
{
	integerKeywordList = value;
}


void GenericQuery::
setStringKwList (char **value)
{
	stringKeywordList = value;
}


void GenericQuery::
setFloatKwList (char **value)
{
	floatKeywordList = value;
}


// make query
int GenericQuery::
makeQuery (ClassAd &ad)
{
	int		i, value;
	char	*item;
	float   fvalue;
	char    req [10240];
	char    buf [1024];
	ExprTree	*tree;
	ClassAdParser 	parser;  // NAC

	// construct query requirement expression
	bool firstCategory = true;
		//sprintf (req, "%s = ", ATTR_REQUIREMENTS);  // NAC
	sprintf(req, ""); // NAC

	// add string constraints
	for (i = 0; i < stringThreshold; i++)
	{
		stringConstraints [i].Rewind ();
		if (!stringConstraints [i].AtEnd ())
		{
			bool firstTime = true;
			strcat (req, firstCategory ? "(" : " && (");
			while ((item = stringConstraints [i].Next ()))
			{
				sprintf (buf, "%s(TARGET.%s == \"%s\")", 
						 firstTime ? " " : " || ", 
						 stringKeywordList [i], item);
				strcat (req, buf);
				firstTime = false;
				firstCategory = false;
			}
			strcat (req, " )");
		}
	}

	// add integer constraints
	for (i = 0; i < integerThreshold; i++)
	{
		integerConstraints [i].Rewind ();
		if (!integerConstraints [i].AtEnd ())
		{
			bool firstTime = true;
			strcat (req, firstCategory ? "(" : " && (");
			while (integerConstraints [i].Next (value))
			{
				sprintf (buf, "%s(TARGET.%s == %d)", 
						 firstTime ? " " : " || ",
						 integerKeywordList [i], value);
				strcat (req, buf);
				firstTime = false;
				firstCategory = false;
			}
			strcat (req, " )");
		}
	}

	// add float constraints
	for (i = 0; i < floatThreshold; i++)
	{
		floatConstraints [i].Rewind ();
		if (!floatConstraints [i].AtEnd ())
		{
			bool firstTime = true;
			strcat (req, firstCategory ? "(" : " && (");
			while (floatConstraints [i].Next (fvalue))
			{
				sprintf (buf, "%s(TARGET.%s == %f)", 
						 firstTime ? " " : " || ",
						 floatKeywordList [i], fvalue);
				strcat (req, buf);
				firstTime = false;
				firstCategory = false;
			}
			strcat (req, " )");
		}
	}

	// add custom AND constraints
	customANDConstraints.Rewind ();
	if (!customANDConstraints.AtEnd ())
	{
		bool firstTime = true;
		strcat (req, firstCategory ? "(" : " && (");
		while ((item = customANDConstraints.Next ()))
		{
			sprintf (buf, "%s(%s)", firstTime ? " " : " && ", item);
			strcat (req, buf);
			firstTime = false;
			firstCategory = false;
		}
		strcat (req, " )");
	}

	// add custom OR constraints
	customORConstraints.Rewind ();
	if (!customORConstraints.AtEnd ())
	{
		bool firstTime = true;
		strcat (req, firstCategory ? "(" : " && (");
		while ((item = customORConstraints.Next ()))
		{
			sprintf (buf, "%s(%s)", firstTime ? " " : " || ", item);
			strcat (req, buf);
			firstTime = false;
			firstCategory = false;
		}
		strcat (req, " )");
	}

	// absolutely no constraints at all
	if (firstCategory) strcat (req, "TRUE");

	// parse constraints and insert into query ad
		//if (Parse (req, tree) > 0) return Q_PARSE_ERROR;   // NAC
		//ad.Insert (tree);
	if ( !( tree = parser.ParseExpression(req) ) ||
		   !(ad.Insert( ATTR_REQUIREMENTS,tree ) ) )  {  // NAC
		return Q_PARSE_ERROR;
	}
	
	return Q_OK;
}


// helper functions --- clear 
void GenericQuery::
clearQueryObject (void)
{
	int i;
	for (i = 0; i < stringThreshold; i++)
		clearStringCategory (stringConstraints[i]);
	
	for (i = 0; i < integerThreshold; i++)
		clearIntegerCategory (integerConstraints[i]);

	for (i = 0; i < floatThreshold; i++)
		clearFloatCategory (floatConstraints[i]);

	clearStringCategory (customANDConstraints);
	clearStringCategory (customORConstraints);
}

void GenericQuery::
clearStringCategory (List<char> &str_category)
{
    char *x;
    str_category.Rewind ();
    while ((x = str_category.Next ()))
    {
        delete [] x;
        str_category.DeleteCurrent ();
    }
}

void GenericQuery::
clearIntegerCategory (SimpleList<int> &int_category)
{
    int item;

    int_category.Rewind ();
    while (int_category.Next (item))
        int_category.DeleteCurrent ();
}

void GenericQuery::
clearFloatCategory (SimpleList<float> &float_category)
{
    float item;

    float_category.Rewind ();
    while (float_category.Next (item))
        float_category.DeleteCurrent ();
}


// helper functions --- copy
void GenericQuery::
copyQueryObject (GenericQuery &from)
{
	int i;

	// copy string constraints
   	for (i = 0; i < stringThreshold; i++)
		copyStringCategory (stringConstraints[i], from.stringConstraints[i]);
	
	// copy integer constraints
	for (i = 0; i < integerThreshold; i++)
		copyIntegerCategory (integerConstraints[i],from.integerConstraints[i]);

	// copy custom constraints
	copyStringCategory (customANDConstraints, from.customANDConstraints);
	copyStringCategory (customORConstraints, from.customORConstraints);

	// copy misc fields
	stringThreshold = from.stringThreshold;
	integerThreshold = from.integerThreshold;
	floatThreshold = from.floatThreshold;

	integerKeywordList = from.integerKeywordList;
	stringKeywordList = from.stringKeywordList;
	floatKeywordList = from.floatKeywordList;
}

void GenericQuery::
copyStringCategory (List<char> &to, List<char> &from)
{
	char *item;

	clearStringCategory (to);
	from.Rewind ();
	while ((item = from.Next ()))
		to.Append (new_strdup (item));
}

void GenericQuery::
copyIntegerCategory (SimpleList<int> &to, SimpleList<int> &from)
{
	int item;

	clearIntegerCategory (to);
	while (from.Next (item))
		to.Append (item);
}

void GenericQuery::
copyFloatCategory (SimpleList<float> &to, SimpleList<float> &from)
{
	float item;

	clearFloatCategory (to);
	while (from.Next (item))
		to.Append (item);
}

// strdup() which uses new
static char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x) return 0;
    strcpy (x, str);
    return x;
}
