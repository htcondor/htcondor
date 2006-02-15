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
#include "condor_common.h"
#include "stream.h"
#include "condor_classad.h"
#include "generic_query.h"
#include "condor_query.h"

/* This file contains various stub functions or small implementation of other
	functions. The purpose of this is to break edges in a nasty dependency
	graph between our .o files so we are able to release the user log API
	to the world. The user log code now uses old classads, and old classads for
	some dumb reason requires cedar. So, I stubbed ceder out right to here
	to avoid dependance on the security libraries and many other things 
	related to that. Then, I made sure to stub the CondorQuery object
	because this little object is responsible for brining in a helluva
	lot of stuff ending at daemoncore.  Since the user of the condorapi
	library doesn't need to talk to a collector to get ads or anything
	they won't miss it. -psilord 02/20/03
	*/


/* if a truly not supportable function is somehow used. Abort. */
static int not_impl(void)
{
	/* this function happens to be included in lincondorapi.a */
	dprintf(D_ALWAYS, 
		"I'm sorry, but this feature is not implemented in libcondorapi.a\n");
	dprintf(D_ALWAYS, 
		"Please see stacktrace in core file to determine culprit.\n");
	dprintf(D_ALWAYS, "Aborting!\n");

	/* this exits */
	abort();

	/* for type convenience return a number */
	return -1;
}

BEGIN_C_DECLS
char* param(const char *str)
{
	if(strcmp(str, "LOG") == 0) {
		return strdup(".");
	}

	return NULL;
}

int param_integer(const char *name, int default_value)
{
	return default_value;
}

END_C_DECLS
int param_integer(const char *name, int default_value, int min, int max)
{
	return default_value;
}

bool param_boolean( const char *name, const bool default_value )
{
	return default_value;
}

/* stub out the various cedar and daemoncore calls I don't need since noone
	should be using them with libcondorapi. Notice: This is gross
	since the header file itself includes partial implementations
	of inline functions and junk. */

/* This is the first non-inline virtual function form the Stream class.
	This means that all of the inline functions get defined in this .o file. :)
	Wheeee!!!! */
Stream::~Stream(){};

/* stub the entire Stream interface :( */
	int Stream::code(void *&foo) { return not_impl(); }
	int Stream::code(char &foo) { return not_impl(); }
	int Stream::code(unsigned char &foo) { return not_impl(); }
	int Stream::code(int &foo){ return not_impl(); }
	int Stream::code(unsigned int &foo){ return not_impl(); }
	int Stream::code(long &foo){ return not_impl(); }
	int Stream::code(unsigned long &foo){ return not_impl(); }
#ifndef WIN32
	int Stream::code(long long &foo){ return not_impl(); }
	int Stream::code(unsigned long long &foo){ return not_impl(); }
#endif

	int Stream::code(short &foo){ return not_impl(); }
	int Stream::code(unsigned short &foo){ return not_impl(); }
	int Stream::code(float &foo){ return not_impl(); }
	int Stream::code(double &foo){ return not_impl(); }
	int Stream::code(char *&foo){ return not_impl(); }
	int Stream::code(char *&foo, int &bar){ return not_impl(); }
	int Stream::code_bytes(void *foo, int bar){ return not_impl(); }
	int Stream::code_bytes_bool(void *foo, int bar){ return not_impl(); }
	int Stream::code(PROC_ID &foo){ return not_impl(); }
	int Stream::code(STARTUP_INFO &foo){ return not_impl(); }
	int Stream::code(PORTS &foo){ return not_impl(); }
	int Stream::code(StartdRec &foo){ return not_impl(); }
	int Stream::code(open_flags_t &foo){ return not_impl(); }
	int Stream::code(struct stat &foo){ return not_impl(); }
	int Stream::code(condor_errno_t &foo){ return not_impl(); }
#if !defined(WIN32)
	int Stream::code(condor_signal_t &foo){ return not_impl(); }
	int Stream::code(fcntl_cmd_t &foo){ return not_impl(); }
	int Stream::code(struct rusage &foo){ return not_impl(); }
	int Stream::code(struct statfs &foo){ return not_impl(); }
	int Stream::code(struct timezone &foo){ return not_impl(); }
	int Stream::code(struct timeval &foo){ return not_impl(); }
	int Stream::code(struct utimbuf &foo){ return not_impl(); }
	int Stream::code(struct rlimit &foo){ return not_impl(); }
	int Stream::code_array(gid_t *&foo, int &bar){ return not_impl(); }
	int Stream::code(struct utsname &foo){ return not_impl(); }
#endif // !defined(WIN32)
#if HAS_64BIT_STRUCTS
	int Stream::code(struct stat64 &foo){ return not_impl(); }
	int Stream::code(struct rlimit64 &foo){ return not_impl(); }
#endif
void Stream::allow_one_empty_message() { not_impl(); }
int Stream::put(char *){ return not_impl(); }
int Stream::get(char *&){ return not_impl(); }

/* stubs for generic query object */
GenericQuery::GenericQuery(void) {}
GenericQuery::~GenericQuery(void) {}

/* stubs for query object. */
QueryResult clearStringConstraints  (const int foo) 
{ return (QueryResult)not_impl();}

QueryResult clearIntegerConstraints (const int foo)
{ return (QueryResult)not_impl();}

QueryResult clearFloatConstraints   (const int foo)
{ return (QueryResult)not_impl();}

void		clearORCustomConstraints(void) 
{ not_impl(); }

void		clearANDCustomConstraints(void) 
{ not_impl(); }

QueryResult CondorQuery::addConstraint (const int foo, const char *bar) 
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addConstraint (const int foo , const int bar)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addConstraint (const int, const float)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addORConstraint (const char *)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::CondorQuery::addANDConstraint (const char *)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::fetchAds (ClassAdList &foo, const char bar[], CondorError* errstack)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::filterAds (ClassAdList &foo, ClassAdList &bar)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::getQueryAd (ClassAd &foo)
{ return (QueryResult)not_impl();}

// overloaded operators

// display
/*friend ostream &CondorQuery::operator<< (ostream &foo, CondorQuery &bar) */
/*	{ return (QueryResult)not_impl();}*/

// assignment
/*CondorQuery	&CondorQuery::operator=  (CondorQuery &)*/
/*	{ return (CondorQuery)not_impl();}*/

CondorQuery::CondorQuery(AdTypes foo) { not_impl();} 

CondorQuery::~CondorQuery() {} 


