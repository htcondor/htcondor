#include "condor_common.h"
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "condor_query.h"
#include "condor_attributes.h"
#include "condor_collector.h"
#include "condor_config.h"
#include "condor_network.h"
#include "condor_parser.h"


#define XDR_ASSERT(x) {if (!(x)) return Q_COMMUNICATION_ERROR;}

char *new_strdup (const char *);

// The order and number of the elements of the following arrays *are*
// important.  (They follow the structure of the enumerations supplied
// in the header file condor_query.h)
const char *ScheddStringKeywords [] = 
{
	ATTR_NAME ,
	ATTR_OWNER 
};

const char *ScheddIntegerKeywords [] = 
{
	ATTR_USERS,
	ATTR_IDLE_JOBS,
	ATTR_RUNNING_JOBS
};

const char *ScheddFloatKeywords [] = 
{	
};

const char *StartdStringKeywords [] = 
{
	ATTR_NAME,
	ATTR_MACHINE,
	ATTR_ARCH,
	ATTR_OPSYS
};

const char *StartdIntegerKeywords [] = 
{
	ATTR_MEMORY,
	ATTR_DISK
};

const char *StartdFloatKeywords [] =
{
};

// normal ctor
CondorQuery::
CondorQuery (AdTypes qType)
{
	queryType = qType;
	switch (qType)
	{
	  case STARTD_AD:
		stringThreshold = STARTD_STRING_THRESHOLD;
		integerThreshold = STARTD_INT_THRESHOLD;
		floatThreshold = STARTD_FLOAT_THRESHOLD;
		command = QUERY_STARTD_ADS;
		break;

	  case SCHEDD_AD:
		stringThreshold = SCHEDD_STRING_THRESHOLD;
		integerThreshold = SCHEDD_INT_THRESHOLD;
		floatThreshold = SCHEDD_FLOAT_THRESHOLD;
		command = QUERY_SCHEDD_ADS;
		break;

	  default:
		stringThreshold = 0;
		integerThreshold = 0;
	}
}


// copy ctor; makes deep copy
CondorQuery::
CondorQuery (const CondorQuery &from)
{
	// discard const due to iterators in internal lists
	copyQueryObject ((CondorQuery &) from);
}


// dtor
CondorQuery::
~CondorQuery ()
{	
	clearQueryObject ();
}


// clear particular string category
QueryResult CondorQuery::
clearStringConstraints (const int i)
{
	if (i >= 0 && i < stringThreshold)
	{
		clearStringCategory (stringConstraints[i]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}


// clear particular integer category
QueryResult CondorQuery::
clearIntegerConstraints (const int i)
{
	if (i >= 0 && i < integerThreshold)
	{
		clearIntegerCategory (integerConstraints[i]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}

// clear particular float category
QueryResult CondorQuery::
clearFloatConstraints (const int i)
{
	if (i >= 0 && i < floatThreshold)
	{
		clearFloatCategory (floatConstraints[i]);
		return Q_OK;
	}
	else
		return Q_INVALID_CATEGORY;
}


void CondorQuery::
clearCustomConstraints (void)
{
	clearStringCategory (customConstraints);
}


// add a string constraint
QueryResult CondorQuery::
addConstraint (const int cat, const char *value)
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


// add an integer constraint
QueryResult CondorQuery::
addConstraint (const int cat, const int value)
{
	if (cat >= 0 && cat < integerThreshold)
	{
		if (!integerConstraints [cat].Append (value))
			return Q_MEMORY_ERROR;
		return Q_OK;
	}

	return Q_INVALID_CATEGORY;
}

// add a float constraint
QueryResult CondorQuery::
addConstraint (const int cat, const float value)
{
	if (cat >= 0 && cat < floatThreshold)
	{
		if (!floatConstraints [cat].Append (value))
			return Q_MEMORY_ERROR;
		return Q_OK;
	}

	return Q_INVALID_CATEGORY;
}

// add a custom constraint
QueryResult CondorQuery::
addConstraint (const char *value)
{
	char *x = new_strdup (value);
	if (!x) return Q_MEMORY_ERROR;
	customConstraints.Append (x);
	return Q_OK;
}


// fetch all ads from the collector that satisfy the constraints
QueryResult CondorQuery::
fetchAds (ClassAdList &adList, const char *poolName)
{
	char 		*pool;
	XDR  		xdr, *xdrs;
	int  		sock = -1, more;
	QueryResult result;
	ClassAd 	query, *ad;

	// use current pool's collector if not specified
	if (poolName[0] == '\0')
	{
		if ((pool = param ("COLLECTOR_HOST")) == NULL) 
			return Q_NO_COLLECTOR_HOST;
	}
	else
		// pool specified
		pool = (char *) poolName;

	// make the query ad
	result = makeQueryAd (query);
	if (result != Q_OK) return result;

	// contact collector
	if((sock=do_connect(pool,"condor_collector",(unsigned)COLLECTOR_PORT)) < 0)
    {
        return Q_COMMUNICATION_ERROR;
    }

    // misc xdr stuff ...
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;
    XDR_ASSERT (xdr_int(xdrs, &command));

	// ship query
	XDR_ASSERT (query.put (xdrs));
	XDR_ASSERT (xdrrec_endofrecord (xdrs, TRUE));
	
	// read in classads
	xdrs->x_op = XDR_DECODE;
	more = 1;
	while (more)
	{
		XDR_ASSERT (xdr_int (xdrs, &more));
		if (more)
		{
			ad = new ClassAd;
			XDR_ASSERT (ad->get (xdrs));
			adList.Insert (ad);
		}
	}

	// finalize
	xdr_destroy (xdrs);
	(void) close (sock);
	
	return (Q_OK);
}


QueryResult CondorQuery::
makeQueryAd (ClassAd &ad)
{
	int		i, value;
	char	*item;
	float   fvalue;
	char 	**stringKeywordList, **integerKeywordList, **floatKeywordList;
	char    req [10240];
	char    buf [1024];
	ExprTree *tree;
	
	// select keyword lists to use
	switch (queryType)
	{
	  case STARTD_AD:  
		stringKeywordList = (char **) StartdStringKeywords;
		integerKeywordList = (char **) StartdIntegerKeywords;
		floatKeywordList = (char **) StartdFloatKeywords;
		ad.SetTargetTypeName ("Machine");
		ad.SetMyTypeName ("Query");
		break;

	  case SCHEDD_AD:
		stringKeywordList = (char **) ScheddStringKeywords;
		integerKeywordList = (char **) ScheddIntegerKeywords;
		floatKeywordList = (char **) ScheddFloatKeywords;
		ad.SetTargetTypeName ("Machine");
		ad.SetMyTypeName ("Query");
		break;

	  default:
		return Q_INVALID_QUERY;
	}

	// construct query requirement expression
	bool firstCategory = true;
	sprintf (req, "%s = ", ATTR_REQUIREMENT);

	// add string constraints
	for (i = 0; i < stringThreshold; i++)
	{
		stringConstraints [i].Rewind ();
		if (!stringConstraints [i].AtEnd ())
		{
			bool firstTime = true;
			strcat (req, firstCategory ? "(" : " && (");
			while (item = stringConstraints [i].Next ())
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

	// add custom constraints
	customConstraints.Rewind ();
	if (!customConstraints.AtEnd ())
	{
		bool firstTime = true;
		strcat (req, firstCategory ? "(" : " && (");
		while (item = customConstraints.Next ())
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
	if (Parse (req, tree) > 0) return Q_PARSE_ERROR;
	ad.Insert (tree);

	return Q_OK;
}


QueryResult CondorQuery::
filterAds (ClassAdList &in, ClassAdList &out)
{
	ClassAd ad, *candidate;
	QueryResult	result;

	// make the query ad
	result = makeQueryAd (ad);
	if (result != Q_OK) return result;

	in.Open();
	while (candidate = (ClassAd *) in.Next())
    {
        // if a match occurs
		if ((*candidate) >= (ad)) out.Insert (candidate);
    }
    in.Close ();
    
	return Q_OK;
}


void CondorQuery::
copyStringCategory (List<char> &to, List<char> &from)
{
	char *item;

	clearStringCategory (to);
	from.Rewind ();
	while (item = from.Next ())
		to.Append (new_strdup (item));
}


void CondorQuery::
copyIntegerCategory (SimpleList<int> &to, SimpleList<int> &from)
{
	int item;

	clearIntegerCategory (to);
	while (from.Next (item))
		to.Append (item);
}

void CondorQuery::
copyFloatCategory (SimpleList<float> &to, SimpleList<float> &from)
{
	float item;

	clearFloatCategory (to);
	while (from.Next (item))
		to.Append (item);
}

void CondorQuery::
clearStringCategory (List<char> &str_category)
{
    char *x;
    str_category.Rewind ();
    while (x = str_category.Next ())
    {
		delete [] x;
		str_category.DeleteCurrent ();
    }
}


void CondorQuery::
clearIntegerCategory (SimpleList<int> &int_category)
{
    int item;

    int_category.Rewind ();
    while (int_category.Next (item))
		int_category.DeleteCurrent ();
}

void CondorQuery::
clearFloatCategory (SimpleList<float> &float_category)
{
    float item;

    float_category.Rewind ();
    while (float_category.Next (item))
		float_category.DeleteCurrent ();
}

// strdup() which uses new
char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x)
    {
		return 0;
    }
    strcpy (x, str);
    return x;
}



// display operator
ostream &operator<< (ostream &out, CondorQuery &q)
{
	char **stringKeywordList, **integerKeywordList;
	int  i;

	switch (q.queryType)
	{  
	  case STARTD_AD:
		stringKeywordList = (char **) StartdStringKeywords;
		integerKeywordList = (char **) StartdIntegerKeywords;
		break;
		
	  case SCHEDD_AD:
		stringKeywordList = (char **) ScheddStringKeywords;
		integerKeywordList = (char **) ScheddIntegerKeywords;
		break;

	  default:
		out << "Invalid query type" << endl;
		return out;
	}

	for (i = 0; i < q.stringThreshold; i++)
	{
		char *item;
		out << stringKeywordList[i] << ":" << endl;
		
		q.stringConstraints[i].Rewind();
		while (item = q.stringConstraints[i].Next())
			out << item << "\t";
		out << endl;
	}

	for (i = 0; i < q.integerThreshold; i++)
	{
		int item;
		out << integerKeywordList[i] << ":" << endl;
		
		q.integerConstraints[i].Rewind();
		while (q.integerConstraints[i].Next(item))
			out << item << "\t";
		out << endl;
	}

	{
		char *item;

		out << "Custom:" << endl;
		q.customConstraints.Rewind();
		while (item = q.customConstraints.Next())
			out << item << "\t";
		out << endl;
	}

	return out;
}


// clear all constraints
void CondorQuery::
clearQueryObject (void)
{
	int i;
	for (i = 0; i < stringThreshold; i++)
		clearStringCategory (stringConstraints[i]);
	
	for (i = 0; i < integerThreshold; i++)
		clearIntegerCategory (integerConstraints[i]);

	clearStringCategory (customConstraints);
}


// make deep copy
void CondorQuery::
copyQueryObject (CondorQuery &from)
{
	int i;

	// current settings are removed during each copy
   
	// copy string constraints
   	for (i = 0; i < stringThreshold; i++)
		copyStringCategory (stringConstraints[i], from.stringConstraints[i]);
	
	// copy integer constraints
	for (i = 0; i < integerThreshold; i++)
		copyIntegerCategory (integerConstraints[i],from.integerConstraints[i]);

	// copy custom constraints
	copyStringCategory (customConstraints, from.customConstraints);

	// copy misc fields
	command = from.command;
	queryType = from.queryType;
	stringThreshold = from.stringThreshold;
	integerThreshold = from.integerThreshold;
}


// assignment operator is just a deep copy
CondorQuery & CondorQuery::
operator= (CondorQuery &rhs)
{
	// current settings are cleared by copy
	copyQueryObject (rhs);
	return rhs;
}
