#include "condor_common.h"
#include <iostream.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "condor_query.h"
#include "condor_collector.h"
#include "condor_status.h"
#include "manager.h" 
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_network.h"
#include "proc_obj.h"
#include "condor_qmgr.h"


#define XDR_ASSERT(x) {if (!(x)) return Q_COMMUNICATION_ERROR;}


// outcomes of get_requirement_expr
enum
{
	NO_ERROR,
	PARSE_ERROR
};


// prototypes of functions used
extern "C" {
	int  xdr_mach_rec( XDR *xdrs, MACH_REC *ptr );
	void free_context (CONTEXT *);
	int  snd_int(XDR*, int, BOOLEAN);
	SetSyscalls(){}
}

char *new_strdup (const char *);
int  get_job_ads (char *, ClassAdList &);

// required by condor_debug
static char *_FileName_ = __FILE__;


// keywords to be used in the classified ad --- the order is important
static char job_str_keywords [][10] =
{
    "Owner",
    "Arch",
    "OpSys",
    "Command",
};


static char job_int_keywords [][10] =
{
    "ClusterId",
    "ProcId",
    "Prio",
    "Status"
};


static char mach_str_keywords [][10] = 
{
    "Machine",
    "Arch",
    "OpSys"
};


static char mach_int_keywords [][10] = 
{
    "Users",
    "Running",
    "Idle",
    "Prio",
};


// function definitions

// vanilla ctor --- List ctors are automatically called
Condor_query::
Condor_query ()
{
}

// copy ctor
Condor_query::
Condor_query (Condor_query& req)
{
    // need to make copies of all lists
    int i;

	// job constraints
	// copy string constraints
	for (i = 0; i < JOB_STRING_THRESHOLD__; i++)
		copy_str_category (job_string_constraints[i], 
						   req.job_string_constraints[i]);
	
	// copy integer constraints
	for (i = 0; i < JOB_INTEGER_THRESHOLD__; i++)
		copy_int_category (job_integer_constraints[i], 
						   req.job_integer_constraints[i]);
        
	// copy custom constraints
	copy_str_category (job_custom_constraints, req.job_custom_constraints);
	
	// machine constraints
	// copy string constraints
	for (i = 0; i < MACHINE_STRING_THRESHOLD__; i++)
		copy_str_category (machine_string_constraints [i], 
						   req.machine_string_constraints [i]);
	
	// copy integer constraints
	for (i = 0; i < MACHINE_INTEGER_THRESHOLD__; i++)
		copy_int_category (machine_integer_constraints [i],
						   req.machine_integer_constraints [i]);
	
	// copy custom constraints
	copy_str_category (machine_custom_constraints, 
					   req.machine_custom_constraints);
}

// helper function to copy a string category
void Condor_query::
copy_str_category (List<char> &to, List<char> &from)
{
	char *item;

	clear_str_category (to);
	from.Rewind ();
	while (item = from.Next ())
		to.Append (new_strdup (item));
}


// helper function to copy an integer category
void Condor_query::
copy_int_category (IntList &to, IntList &from)
{
	int item;

	clear_int_category (to);
	while (from.Next (item))
		to.Append (item);
}


// dtor
Condor_query::
~Condor_query ()
{
    int i;

    // job constraints
    // clear string categories
    for (i = 0; i < JOB_STRING_THRESHOLD__; i++)
        clear ((job_str_category) i);

    // clear integer categories
    for (i = 0; i < JOB_INTEGER_THRESHOLD__; i++)
        clear ((job_int_category) i);

    // clear JOB_CUSTOM category
    clear (JOB_CUSTOM);

    // machine categories
    // clear string categories
    for (i = 0; i < MACHINE_STRING_THRESHOLD__; i++)
        clear ((machine_str_category) i);

    // clear integer categories
    for (i = 0; i < MACHINE_INTEGER_THRESHOLD__; i++)
        clear ((machine_int_category) i);

    // clear MACHINE_CUSTOM category
    clear (MACHINE_CUSTOM);
}



// -------------- clear categories --------------

// helper function to clear a string category
void Condor_query::
clear_str_category (List<char> &str_category)
{
    char *x;
    str_category.Rewind ();
    while (x = str_category.Next ())
    {
		delete [] x;
		str_category.DeleteCurrent ();
    }
}

// helper function to clear an integer category
void Condor_query::
clear_int_category (IntList &int_category)
{
    int item;

    int_category.Rewind ();
    while (int_category.Next (item))
		int_category.DeleteCurrent ();
}


// job constraints
// clear a string category
void Condor_query::
clear (const job_str_category cat)
{
    if (cat < JOB_STRING_THRESHOLD__ && cat >= 0)
		clear_str_category (job_string_constraints [cat]);
}

// clear an integer category
void Condor_query::
clear (const job_int_category cat)
{
    if (cat < JOB_INTEGER_THRESHOLD__ && cat >= 0)
		clear_int_category (job_integer_constraints [cat]);
}

// clear all custom constraints
void Condor_query::
clear (const job_cust_category cat)
{
    clear_str_category (job_custom_constraints);
}


// machine constraints
// clear a string category
void Condor_query::
clear (const machine_str_category cat)
{
    if (cat < MACHINE_STRING_THRESHOLD__ && cat >= 0)
		clear_str_category (machine_string_constraints [cat]);
}

// clear an integer category
void Condor_query::
clear (const machine_int_category cat)
{
    if (cat < MACHINE_INTEGER_THRESHOLD__ && cat >= 0)
		clear_int_category (machine_integer_constraints [cat]);
}

// clear all custom constraints
void Condor_query::
clear (const machine_cust_category cat)
{
    clear_str_category (machine_custom_constraints);
}


//---------------------------------------------


// ------------- add constraints -------------- 
// job categories
// add a string constraint to a string category
void Condor_query::
add (const job_str_category cat, const char *value)
{
    if (cat < JOB_STRING_THRESHOLD__ && cat >= 0)
         job_string_constraints [cat].Append (new_strdup (value));
}

// add an integer constraint to an integer category
void Condor_query::
add (const job_int_category cat, const int value)
{
    if (cat < JOB_INTEGER_THRESHOLD__ && cat >= 0)
    {
        job_integer_constraints [cat].Append ((int) value);
    }
}

// add a custom constraint
void Condor_query::
add (const job_cust_category cat, const char *value)
{
    job_custom_constraints.Append (new_strdup (value));
}


// machine categories
// add a string constraint to a string category
void Condor_query::
add (const machine_str_category cat, const char *value)
{
    if (cat < MACHINE_STRING_THRESHOLD__ && cat >= 0)
        machine_string_constraints [cat].Append (new_strdup (value));
}

// add an integer constraint to an inetegr category
void Condor_query::
add (const machine_int_category cat, const int value)
{
    if (cat < MACHINE_INTEGER_THRESHOLD__ && cat >= 0)
        machine_integer_constraints [cat].Append ((int) value);
}

// add a custom constraint
void Condor_query::
add (const machine_cust_category cat, const char *value)
{
    machine_custom_constraints.Append (new_strdup (value));
}


//-----------------------------------------------
int Condor_query::
condor_status (ClassAdList &machine_list, const char pool_name [])
{
    char        *collector_host;
    int         sock = -1, more;
    XDR         xdr, *xdrs = NULL;
    int         cmd;
    ClassAdList candidate_ad_list;
    ClassAd     *machine_ad;
    bool        hosts_specified = false;
	int         result;
	ClassAd     query;

	// check if the requirements can be made into a class ad
	if ((result = make_machine_query_ad (query)) == PARSE_ERROR)
		return Q_PARSE_MACH_REQS;

    // get name of machine on which the collector resides
    if (*pool_name)
    {
		// pool name supplied
		collector_host = (char *) pool_name;
    }
    else
    {
		// get pool name from config file
		if((collector_host = param("COLLECTOR_HOST")) == NULL)
		{
			dprintf(D_ALWAYS, "COLLECTOR_HOST not specified in config file\n");
			return (1);
		}
    }

    // connect to the collector
    if((sock=do_connect(collector_host, "condor_collector", COLLECTOR_PORT))<0)
    {
        dprintf(D_ALWAYS, "Can't connect to Condor Collector\n");
        return 1;
    }

    // misc xdr stuff ...
    xdrs = xdr_Init( &sock, &xdr );
    xdrs->x_op = XDR_ENCODE;
    cmd = QUERY_STARTD_ADS;
    XDR_ASSERT( xdr_int(xdrs, &cmd) );
	// ship query
	XDR_ASSERT (query.put (xdrs));
	XDR_ASSERT (xdrrec_endofrecord (xdrs, TRUE));

	// read in contexts, convert to machine ads and store in ClassAd list
	xdrs->x_op = XDR_DECODE;
	more = 1;
	while (more)
	{
		XDR_ASSERT (xdr_int (xdrs, &more));
		if (more)
		{
			machine_ad = new ClassAd;
			XDR_ASSERT (machine_ad->get (xdrs));
			machine_list.Insert (machine_ad);
		}
	}

	// finalize
	xdr_destroy (xdrs);
	(void) close (sock);

	return (OK);
}


 
int Condor_query::
condor_queue (ClassAdList &job_ad_list, const char *pool_name)
{
    ClassAdList machine_ads;
	int result;

    // get list of machine ads
    result = condor_status (machine_ads, pool_name);
	if (result != OK)
		return result;
	else
		return (condor_queue (machine_ads, job_ad_list));
}


int Condor_query::
condor_queue (ClassAdList &machine_ads, ClassAdList &job_ad_list)
{
	ClassAdList candidate_job_ads;
    ClassAd     *mach_ad;
    char        ip_addr [25];
    char        schedd_addr [20];
    ExprTree    *IP_PORT, *machine_name;
	
    // for each machine that we need to contact ...
	// HACK:: For now, ClassAdList is defined as an AttrListList, so need cast
    machine_ads.Open ();
    while (mach_ad = (ClassAd *) machine_ads.Next ())
    {
		// get the IP:port of the schedd
		IP_PORT = mach_ad->Lookup ("SCHEDD_IP_ADDR");
		if (IP_PORT && IP_PORT->RArg())
		{
			// get the string "<xxx.xxx.xxx.xxx:pppp>"
			strcpy (ip_addr, ((String *)IP_PORT->RArg())->Value());
		}
		else
		{
			// error in getting the SCHEDD_IP_ADDR
			continue;
		}
		
		// contact machine and get job ads
		get_job_ads (schedd_addr, job_ad_list);
    }
    machine_ads.Close ();
	
    // filter the job ads and return
    return (filter_jobs (candidate_job_ads, job_ad_list));
}


// get_queue for filtering a ClassAdList
int Condor_query::
filter_jobs (ClassAdList &ad_list, ClassAdList &new_list)
{
    ClassAd*    candidate_ad;
    ClassAd     class_ad;
	int         result;

	if ((result = make_job_query_ad (class_ad)) == PARSE_ERROR)
		// error
		return Q_PARSE_JOB_REQS;

    // scan thru list for jobs matching the query
	// HACK:: same hack as previously described
    ad_list.Open();
    while (candidate_ad = (ClassAd *) ad_list.Next())
    {
        // if no constraints were specified, or a match occurs
        if ((*candidate_ad) >= (class_ad))
			new_list.Insert (candidate_ad);
	}
    ad_list.Close ();
    
    // no error
    return OK;
}


int Condor_query::
filter_machines (ClassAdList &ad_list, ClassAdList &new_list)
{
    ClassAd*    candidate_ad;
    ClassAd     class_ad;
	int         result;

	if ((result = make_machine_query_ad (class_ad)) == PARSE_ERROR)
		// error
		return Q_PARSE_MACH_REQS;

    // scan thru list for jobs matching the query
	// HACK:: same hack as previously described
    ad_list.Open();
    while (candidate_ad = (ClassAd *) ad_list.Next())
    {
        // if no constraints were specified, or a match occurs
        if ((*candidate_ad) >= (class_ad))
			new_list.Insert (candidate_ad);
	}
    ad_list.Close ();
    
    // no error
    return OK;
}


int Condor_query::
make_machine_query_ad (ClassAd &class_ad)
{
	char *requirements = get_machine_expr_string ();
	ExprTree *expr_tree;
	
	// check if requirements were specified
	if (*requirements)
    {
        // check for parse error
        int err_pos;
        if ((err_pos = Parse (requirements, expr_tree)) > 0)
        {
            cerr << "parse error at " << err_pos << endl;
            cerr << "string is: " << requirements << endl;
            delete [] requirements;
            return PARSE_ERROR;
        }

		delete [] requirements;
	}
	else
	{
		Parse ("Requirement = TRUE", expr_tree);
	}

	// insert trees and set types
	class_ad.Insert (expr_tree);
	class_ad.SetMyTypeName ("MachineQuery");
	class_ad.SetTargetTypeName ("Machine");

	return NO_ERROR;
}

int Condor_query::
make_job_query_ad (ClassAd &class_ad)
{
	char *requirements = get_job_expr_string ();
	ExprTree *expr_tree;
	
	// check if requirements were specified
	if (*requirements)
    {
        // check for parse error
        int err_pos;
        if ((err_pos = Parse (requirements, expr_tree)) > 0)
        {
            cerr << "parse error at " << err_pos << endl;
            cerr << "string is: " << requirements << endl;
            delete [] requirements;
            return PARSE_ERROR;
        }

		delete [] requirements;
	}
	else
	{
		Parse ("Requirement = TRUE", expr_tree);
	}

	// insert trees and set types
	class_ad.Insert (expr_tree);
	class_ad.SetMyTypeName ("JobQuery");
	class_ad.SetTargetTypeName ("Job");
	
	return NO_ERROR;
}


//--------------------------------------------------


//--------------- overloaded ops -------------------

ostream& operator<< (ostream& out, Condor_query &req)
{
    int i;
	char *item;
   
    // output job string constraints
    for (i = 0; i < JOB_STRING_THRESHOLD__; i++)
    {
        char *item;
        out << job_str_keywords [i] << ":" << endl;
        req.job_string_constraints [i].Rewind ();
        while (item = req.job_string_constraints [i].Next ())
            out << "\t" << item;
        out << endl << endl;
    }

    // output job integer constraints
    for (i = 0; i < JOB_INTEGER_THRESHOLD__; i++)
    {
        int item;
        out << job_int_keywords [i] << ":" << endl;
        req.job_integer_constraints [i].Rewind ();
        while (req.job_integer_constraints [i].Next(item))
            out << "\t" << item;
        out << endl << endl;
    }

    // output job custom constraints
    out << "Job custom:" << endl;
    req.job_custom_constraints.Rewind ();
    while (item = req.job_custom_constraints.Next ())
        out << "\t" << item;
    out << endl << endl;

    // output machine string constraints
    for (i = 0; i < MACHINE_STRING_THRESHOLD__; i++)
    {
        char *item;
        out << mach_str_keywords [i] << ":" << endl;
        req.machine_string_constraints [i].Rewind ();
        while (item = req.machine_string_constraints [i].Next ())
            out << "\t" << item;
        out << endl << endl;
    }

    // output machine integer constraints
    for (i = 0; i < MACHINE_INTEGER_THRESHOLD__; i++)
    {
        int item;
        out << mach_int_keywords [i] << ":" << endl;
        req.machine_integer_constraints [i].Rewind ();
        while (req.machine_integer_constraints [i].Next(item))
            out << "\t" << item;
        out << endl << endl;
    }

    // output machine custom constraints
    out << "Machine customs:" << endl;
    req.machine_custom_constraints.Rewind ();
    while (item = req.machine_custom_constraints.Next ())
        out << "\t" << item;
    out << endl << endl;

    // return object
    return out;
}
                

// assignment operator --- very similar to copy constructor
Condor_query & Condor_query::
operator= (Condor_query & req)
{
    // need to make copies of all lists
    int i;

    // make assignment iff lhs is not the rhs
    if (this != &req)
    {    
		// need to make copies of all lists
		int i;
		
		// job constraints
		// copy string constraints
		for (i = 0; i < JOB_STRING_THRESHOLD__; i++)
			copy_str_category (job_string_constraints[i], 
							   req.job_string_constraints[i]);
		
		// copy integer constraints
		for (i = 0; i < JOB_INTEGER_THRESHOLD__; i++)
			copy_int_category (job_integer_constraints[i], 
							   req.job_integer_constraints[i]);
        
		// copy custom constraints
		copy_str_category (job_custom_constraints, 
						   req.job_custom_constraints);
		
		// machine constraints
		// copy string constraints
		for (i = 0; i < MACHINE_STRING_THRESHOLD__; i++)
			copy_str_category (machine_string_constraints [i], 
							   req.machine_string_constraints [i]);
		
		// copy integer constraints
		for (i = 0; i < MACHINE_INTEGER_THRESHOLD__; i++)
			copy_int_category (machine_integer_constraints [i],
							   req.machine_integer_constraints [i]);
		
		// copy custom constraints
		copy_str_category (machine_custom_constraints, 
						   req.machine_custom_constraints);
	}

    // return object
    return *this;
}


//------------ get the requirement expr ------------
char *Condor_query::
get_job_expr_string (void)
{
    char *req_string;
    int   i;
    bool  first_category = true;

    // the requirement string should be less than 10k
    req_string = new char [10240 + 1];
    if (!req_string)
    {
        cerr<<"memory alloc error: Condor_query::get_expr_string ()"<<endl;
        exit (1);
    }
    strcpy (req_string, "Requirement = ");

    // create the string constraints
    for (i = 0; i < JOB_STRING_THRESHOLD__; i++)
    {
        job_string_constraints [i].Rewind();
        if (!job_string_constraints [i].AtEnd())
        {
            char *item;

            // at least one item in the list
            // check if this is the first category to be non-empty
            if (first_category)
                first_category = false;
            else
                strcat (req_string, " && ");

            // delimit category
            strcat (req_string, "(");

            // scan thru list
            bool first_time = true;
            while (item = job_string_constraints [i].Next ())
            {
                if (!first_time)
                    strcat (req_string, " || ");

                // remove strlen and keep track of running length for a more
                // efficient implementation
                sprintf (req_string + strlen (req_string), 
                         "(TARGET.%s == \"%s\")", job_str_keywords [i], item);

                first_time = false;
            }

            // delimit category
            strcat (req_string, ")");
        }
    }

    // perform operation for integer constraints as well
    for (i = 0; i < JOB_INTEGER_THRESHOLD__; i++)
    {
        job_integer_constraints [i].Rewind ();
        if (!job_integer_constraints [i].AtEnd ())
        {
            int item;

            // at least one item in the list
            // check if this is the first category to be non-empty
            if (first_category)
                first_category = false;
            else
                strcat (req_string, " && ");

            // delimit category
            strcat (req_string, "(");

            // scan thru list
            bool first_time = true;
            while (job_integer_constraints [i].Next (item))
            {
                if (!first_time)
                    strcat (req_string, " || ");

                // remove strlen and keep track of running length for a more
                // efficient implementation
                sprintf (req_string + strlen (req_string), 
                         "(TARGET.%s == %d)", job_int_keywords [i], item);

                first_time = false;
            }

            // delimit category
            strcat (req_string, ")");
        }
    }

    // finally, perform the operation for custom constraints
    job_custom_constraints.Rewind ();
    if (!job_custom_constraints.AtEnd())
    {
        char *item;

        // at least one item in the list
        // check if this is the first category to be non-empty
        if (!first_category)
            strcat (req_string, " && ");
        else
            first_category = false;

        // delimit category
        strcat (req_string, "(");

        // scan thru list
        bool first_time = true;
        while (item = job_custom_constraints.Next ())
        {
            if (!first_time)
                strcat (req_string, " || ");

            // add custom constraint
            sprintf (req_string + strlen (req_string), "(%s)", item);
        
            first_time = false;
        }

        // delimit category
        strcat (req_string, ")");
    }

    // check if anything was added at all
    if (first_category)
        // else return an empty requirement string
        *req_string = '\0';
      
    // return the requirement string
    return req_string;
}


char *Condor_query::
get_machine_expr_string (void)
{
    char *req_string;
    int   i;
    bool  first_category = true;

    // the requirement string should be less than 10k
    req_string = new char [10240 + 1];
    if (!req_string)
    {
        cerr<<"memory alloc error: Condor_query::get_expr_string ()"<<endl;
        exit (1);
    }
    strcpy (req_string, "Requirement = ");

    // create the string constraints
    for (i = 0; i < MACHINE_STRING_THRESHOLD__; i++)
    {
        machine_string_constraints [i].Rewind();
        if (!machine_string_constraints [i].AtEnd())
        {
            char *item;

            // at least one item in the list
            // check if this is the first category to be non-empty
            if (first_category)
                first_category = false;
            else
                strcat (req_string, " && ");

            // delimit category
            strcat (req_string, "(");

            // scan thru list
            bool first_time = true;
            while (item = machine_string_constraints [i].Next ())
            {
                if (!first_time)
                    strcat (req_string, " || ");

                // remove strlen and keep track of running length for a more
                // efficient implementation
                sprintf (req_string + strlen (req_string), 
                         "(TARGET.%s == \"%s\")", mach_str_keywords [i], item);

                first_time = false;
            }

            // delimit category
            strcat (req_string, ")");
        }
    }

    // perform operation for integer constraints as well
    for (i = 0; i < MACHINE_INTEGER_THRESHOLD__; i++)
    {
        machine_integer_constraints [i].Rewind ();
        if (!machine_integer_constraints [i].AtEnd ())
        {
            int item;

            // at least one item in the list
            // check if this is the first category to be non-empty
            if (first_category)
                first_category = false;
            else
                strcat (req_string, " && ");

            // delimit category
            strcat (req_string, "(");

            // scan thru list
            bool first_time = true;
            while (machine_integer_constraints [i].Next (item))
            {
                if (!first_time)
                    strcat (req_string, " || ");

                // remove strlen and keep track of running length for a more
                // efficient implementation
                sprintf (req_string + strlen (req_string), 
                         "(TARGET.%s == %d)", mach_int_keywords [i], item);

                first_time = false;
            }

            // delimit category
            strcat (req_string, ")");
        }
    }

    // finally, perform the operation for custom constraints
    machine_custom_constraints.Rewind ();
    if (!machine_custom_constraints.AtEnd())
    {
        char *item;

        // at least one item in the list
        // check if this is the first category to be non-empty
        if (!first_category)
            strcat (req_string, " && ");
        else
            first_category = false;

        // delimit category
        strcat (req_string, "(");

        // scan thru list
        bool first_time = true;
        while (item = machine_custom_constraints.Next ())
        {
            if (!first_time)
                strcat (req_string, " || ");

            // add custom constraint
            sprintf (req_string + strlen (req_string), "(%s)", item);
        
            first_time = false;
        }

        // delimit category
        strcat (req_string, ")");
    }

    // check if anything was added at all
    if (first_category)
        // else return an empty requirement string
        *req_string = '\0';
      
    // return the requirement string
    return req_string;
}


int 
get_job_ads (char *hostname, ClassAdList &ad_list)
{
    ClassAd*    	jobAd;
    Qmgr_connection *qmgr;
	int             cluster = -1, proc = -1;
	int             next_cluster, next_proc;
	int             rval, error = 0;
	char            exprName[1000], exprValue[1000];
	ExprTree	    *tree;

	// connect to the queue
	if ((qmgr = ConnectQ(hostname)) == 0) 
	{
		return 1;
	}

	//  for each job ...
	rval = GetNextJob (cluster, proc, &next_cluster, &next_proc);
	while (rval >= 0)
	{
		jobAd  = new ClassAd;
		if (jobAd == 0)
		{
			// memory allocation error
			error = -1;
			break;
		}

		// read in the attributes
		rval = FirstAttribute (cluster, proc, exprName);
		while (rval >= 0)
		{
			rval = GetAttributeExpr (cluster, proc, exprName, exprValue);
			if (rval >= 0 && Parse (exprValue, tree) == 0)
			{
				jobAd->Insert (tree);
			}
			rval = NextAttribute (cluster, proc, exprName);
		} 

		// set types
		jobAd->SetMyTypeName ("Job");
		jobAd->SetTargetTypeName ("Machine");

		// insert ad into ad list
		ad_list.Insert (jobAd);

		// get the next job in the queue
		cluster = next_cluster;
		proc = next_proc;
		rval = GetNextJob (cluster, proc, &next_cluster, &next_proc);
	}
		
	DisconnectQ (qmgr);
	return error;
}


// strdup() which uses new
char *new_strdup (const char *str)
{
    char *x = new char [strlen (str) + 1];
    if (!x)
    {
        cerr << "memory alloc error:  tried alloc for: " << str << endl;
        exit (1);
    }
    strcpy (x, str);
    return x;
}
