/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "generic_query.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include <algorithm>
#include <numeric>
#include <iterator>

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

	floatKeywordList = NULL;
	integerKeywordList = NULL;
	stringKeywordList = NULL;
}


GenericQuery::
GenericQuery (const GenericQuery &gq)
{
	// initialize category counts
	integerThreshold = 0;
	stringThreshold = 0;
	floatThreshold = 0;

	// initialize pointers
	integerConstraints = 0;
	floatConstraints = 0;
	stringConstraints = 0;

	floatKeywordList = NULL;
	integerKeywordList = NULL;
	stringKeywordList = NULL;

	copyQueryObject(gq);
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
		integerConstraints = new std::vector<int> [integerThreshold];
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
		floatConstraints = new std::vector<float> [floatThreshold];
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
        integerConstraints [cat].push_back(value);
        return Q_OK;
    }

    return Q_INVALID_CATEGORY;
}

int GenericQuery::
addFloat (const int cat, float value)
{
    if (cat >= 0 && cat < floatThreshold)
    {
        floatConstraints [cat].push_back(value);
        return Q_OK;
    }

    return Q_INVALID_CATEGORY;
}

int GenericQuery::
addString (const int cat, const char *value)
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

bool GenericQuery::
hasString(const int cat, const char * value)
{
	if (cat >= 0 && cat < stringThreshold) {
		return hasItem(stringConstraints[cat], value);
	}
	return false;
}

bool GenericQuery::
hasStringNoCase(const int cat, const char * value)
{
	if (cat >= 0 && cat < stringThreshold) {
		return hasItemNoCase(stringConstraints[cat], value);
	}
	return false;
}


int GenericQuery::
addCustomOR (const char *value)
{
	if (hasCustomOR(value)) return Q_OK;
	char *x = new_strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customORConstraints.Append (x);
	return Q_OK;
}

int GenericQuery::
addCustomAND (const char *value)
{
	if (hasCustomAND(value)) return Q_OK;
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
makeQuery (std::string &req)
{
	int		i;
	char	*item;

	req = "";

	// construct query requirement expression
	bool firstCategory = true;

	// add string constraints
	for (i = 0; i < stringThreshold; i++)
	{
		stringConstraints [i].Rewind ();
		if (!stringConstraints [i].AtEnd ())
		{
			bool firstTime = true;
			req += firstCategory ? "(" : " && (";
			while ((item = stringConstraints [i].Next ()))
			{
				formatstr_cat (req, "%s(%s == \"%s\")", 
						 firstTime ? " " : " || ", 
						 stringKeywordList [i], item);
				firstTime = false;
				firstCategory = false;
			}
			req += ')';
		}
	}

	// add integer constraints
	for (i = 0; i < integerThreshold; i++)
	{
		if (!integerConstraints[i].empty()) {
			bool firstTime = true;
			req += firstCategory ? "(" : " && (";
			for (int ic : integerConstraints[i]) {
				formatstr_cat (req, "%s(%s == %d)", 
						 firstTime ? " " : " || ",
						 integerKeywordList [i], ic);
				firstTime = false;
				firstCategory = false;
			}
			req += ')';
		}
	}

	// add float constraints
	for (i = 0; i < floatThreshold; i++)
	{
		if (!floatConstraints[i].empty()) {
			bool firstTime = true;
			req += firstCategory ? "(" : " && (";
			for (float fvalue: floatConstraints[i]) {
				formatstr_cat (req, "%s(%s == %f)", 
						firstTime ? " " : " || ",
						floatKeywordList [i], fvalue);
				firstTime = false;
				firstCategory = false;
			}
			req += ')';
		}
	}

	// add custom AND constraints
	customANDConstraints.Rewind ();
	if (!customANDConstraints.AtEnd ())
	{
		bool firstTime = true;
		req += firstCategory ? "(" : " && (";
		while ((item = customANDConstraints.Next ()))
		{
			formatstr_cat (req, "%s(%s)", firstTime ? " " : " && ", item);
			firstTime = false;
			firstCategory = false;
		}
		req += " )";
	}

	// add custom OR constraints
	customORConstraints.Rewind ();
	if (!customORConstraints.AtEnd ())
	{
		bool firstTime = true;
		req += firstCategory ? "(" : " && (";
		while ((item = customORConstraints.Next ()))
		{
			formatstr_cat (req, "%s(%s)", firstTime ? " " : " || ", item);
			firstTime = false;
			firstCategory = false;
		}
		req += " )";
	}

	return Q_OK;
}

int GenericQuery::
makeQuery (ExprTree *&tree)
{
	std::string req;
	int status = makeQuery(req);
	if (status != Q_OK) return status;

	// If there are no constraints, then we match everything.
	if (req.empty()) req = "TRUE";

	// parse constraints and insert into query ad
	if (ParseClassAdRvalExpr (req.c_str(), tree) > 0) return Q_PARSE_ERROR;

	return Q_OK;
}

// helper functions --- clear 
void GenericQuery::
clearQueryObject (void)
{
	int i;
	for (i = 0; i < stringThreshold; i++)
		if (stringConstraints) clearStringCategory (stringConstraints[i]);
	
	for (i = 0; i < integerThreshold; i++)
		if (integerConstraints) clearIntegerCategory (integerConstraints[i]);

	for (i = 0; i < floatThreshold; i++)
		if (integerConstraints) clearFloatCategory (floatConstraints[i]);

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
clearIntegerCategory (std::vector<int> &int_category)
{
	int_category.clear();
}

void GenericQuery::
clearFloatCategory (std::vector<float> &float_category)
{
	float_category.clear();
}


// helper functions --- copy
void GenericQuery::
copyQueryObject (const GenericQuery &from)
{
	int i;

	// copy string constraints
   	for (i = 0; i < from.stringThreshold; i++)
		if (stringConstraints) copyStringCategory (stringConstraints[i], from.stringConstraints[i]);
	
	// copy integer constraints
	for (i = 0; i < from.integerThreshold; i++)
		if (integerConstraints) copyIntegerCategory (integerConstraints[i],from.integerConstraints[i]);

	// copy custom constraints
	copyStringCategory (customANDConstraints, const_cast<List<char> &>(from.customANDConstraints));
	copyStringCategory (customORConstraints, const_cast<List<char> &>(from.customORConstraints));

	// copy misc fields
	stringThreshold = from.stringThreshold;
	integerThreshold = from.integerThreshold;
	floatThreshold = from.floatThreshold;

	integerKeywordList = from.integerKeywordList;
	stringKeywordList = from.stringKeywordList;
	floatKeywordList = from.floatKeywordList;

	floatConstraints = from.floatConstraints;
	integerConstraints = from.integerConstraints;
	stringConstraints = from.stringConstraints;
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
copyIntegerCategory (std::vector<int> &to, std::vector<int> &from)
{
	clearIntegerCategory (to);
	std::copy(from.begin(), from.end(), std::back_inserter(to));
}

void GenericQuery::
copyFloatCategory (std::vector<float> &to, std::vector<float> &from)
{
	clearFloatCategory (to);
	std::copy(from.begin(), from.end(), std::back_inserter(to));
}

// strdup() which uses new
static char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x) return 0;
    strcpy (x, str);
    return x;
}
