#include "condor_common.h"
#include "caseSensitivity.h"
#include "condor_string.h"
#include "classad.h"

extern "C" void to_lower (char *);	// from util_lib (config.c)


Attribute::
Attribute ()
{
	valid = false;
	attrName = NULL;
	expression = NULL;
}


ClassAd::
ClassAd ()
{
	Attribute	dummy;

	// initialize the attribute list
	dummy.valid = false;
	dummy.attrName = NULL;
	dummy.expression = NULL;

	attrList.fill (dummy);

	// initialize additional internal state
	schema = NULL;
	last = 0;
}


ClassAd::
ClassAd (char *domainName)
{
	int domainIndex;
	Attribute attr;

	// get pointer to schema and stash it for easy access
	domMan.getDomainSchema (domainName, domainIndex, schema);

	// initialize other internal structures
	attr.valid = false;
	attr.attrName = NULL;
	attr.expression = NULL;
	attrList.fill (attr);
	last = 0;	
}


ClassAd::
ClassAd (const ClassAd &ad) : attrList (ad.attrList)
{
	schema = ad.schema;
	last = ad.last;
}	


ClassAd::
~ClassAd ()
{
	// not domainized
	if (!schema)
	{
		for (int i = 0; i < last; i++)
		{
			if (attrList[i].attrName) free(attrList[i].attrName);
			if (attrList[i].expression) delete attrList[i].expression;
		}
	}
}


bool ClassAd::
insert (char *name, ExprTree *tree)
{
	int			index;
	char		*attr = NULL;

	// sanity check
	if (!name || !tree) return false;

	// if the classad is domainized
	if (schema)
	{
		SSString s;

		// operate on the common schema array
		index = schema->getCanonical (name, s);
		attrList[index].canonicalAttrName.copy(s);
	}
	else
	{
		// use the attrlist as a standard unordered list
		for (index = 0; index < last; index ++)
		{
			if (CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name) == 0)
				break;
		}
		if (index == last) 
		{
			attr = strdup (name);
			last++;
		}
	}

	// if inserting for the first time
	if (attrList[index].valid == false)
	{
		if (index > last) last = index;

		attrList[index].valid = true;
		attrList[index].attrName = attr;
		attrList[index].expression = tree;
		
		return true;
	}

	// if a tree was previously inserted here, delete it	
	if (attrList[index].expression) 
	{
		delete (attrList[index].expression);
	}
	
	// insert the new tree
	attrList[index].expression = tree;

	return false;
}


ExprTree *ClassAd::
lookup (char *name)
{
	int			index;

	// sanity check
	if (!name) return NULL;

    // if the classad is domainized
    if (schema)
    {
        // operate on the common schema array
        index = schema->getCanonical (name);
    }
    else
    {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++)
        {
            if (CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name) == 0) 
				break;
        }
    }

	return (attrList[index].valid ? attrList[index].expression : NULL);
}


bool ClassAd::
remove (char *name)
{
    int         index;

	// sanity check
	if (!name) return false;

    // if the classad is domainized
    if (schema)
    {
        // operate on the common schema array
        index = schema->getCanonical (name);
    }
    else
    {
        // use the attrlist as a standard unordered list
        for (index = 0; index < last; index ++)
        {
            if (CLASSAD_ATTR_NAMES_STRCMP(attrList[index].attrName, name) == 0)
			{
				free (attrList[index].attrName);
				last --;
				attrList[index] = attrList[last];
				index = last;
				
				break;
			}
        }
    }

	if (attrList[index].valid)
	{
		attrList[index].valid = false;
		delete attrList[index].expression;

		return true;
	}

	return false;
}


bool ClassAd::
toSink (Sink &s)
{
	char	*attrName;
	bool    first = true;

	if (!s.sendToSink ((void*)"[\n", 2)) return false;

	for (int index = 0; index < last; index++)
	{
		if (attrList[index].valid && attrList[index].attrName && 
			attrList[index].expression)
		{
			attrName=schema ? attrList[index].canonicalAttrName.getCharString()
							: attrList[index].attrName;

			// if this is not the first attribute, print the ";" separator
			if (!first) 
			{
				if (!s.sendToSink ((void*)";\n", 2)) return false;
			}

			if (!s.sendToSink ((void*)attrName, strlen(attrName))	||
				!s.sendToSink ((void*)" = ", 3)						||
				!(attrList[index].expression)->toSink(s))
					return false;

			first = false;
		}
	}

	return (s.sendToSink ((void*)"\n]\n", 3));
}


ClassAd *ClassAd::
fromSource (Source &s)
{
	ClassAd	*ad = new ClassAd;

	if (!ad) return NULL;
	if (!s.parseClassAd (*ad)) 
	{
		delete ad;
		return (ClassAd*)NULL;
	}

	// ... else 
	return ad;
}

ClassAd *ClassAd::
augmentFromSource (Source &s, ClassAd &ad)
{
    return (s.parseClassAd(ad) ? &ad : NULL);
}



bool ClassAdIterator::
nextAttribute (char *&attr, ExprTree *&expr)
{
	if (!ad) return false;

	index++;
	while (index < ad->last && !(ad->attrList[index].valid)) index++;
	if (index == ad->last) return false;
	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
	expr = ad->attrList[index].expression;
	return true;
}


bool ClassAdIterator::
previousAttribute (char *&attr, ExprTree *&expr)
{
	if (!ad) return false;

    index--;
    while (index > -1 && !(ad->attrList[index].valid)) index--;
    if (index == -1) return false;
	attr = (ad->schema) ? ad->attrList[index].canonicalAttrName.getCharString()
					: ad->attrList[index].attrName;
    expr = ad->attrList[index].expression;
	return true;	
}

