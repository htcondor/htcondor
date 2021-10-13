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

#include "condor_common.h"
#include "stream.h"
#include "condor_classad.h"
#include "generic_query.h"
#include "condor_query.h"

#ifdef WIN32
bool 
get_password_from_credd (
	const char * credd_host,
	const char  username[],
	const char  domain[],
	char * pw,
	int    cb) // sizeof pw buffer (in bytes)
{
    return false;
}
bool 
cache_credd_locally (
	const char  username[],
	const char  domain[],
	const char * pw)
{
	return false;
}
#endif
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

int condor_fsync(int, const char*)
{
	return 0;
}

int condor_fdatasync(int, const char*)
{
	return 0;
}

BEGIN_C_DECLS
void
config( int, bool )
{
}

char* param(const char *str)
{
	if(strcmp(str, "LOG") == 0) {
		return strdup(".");
	}

	return NULL;
}

char* param_without_default(const char *str)
{
	if(strcmp(str, "LOG") == 0) {
		return strdup(".");
	}
	return NULL;
}

int param_integer(const char *, int default_value)
{
	return default_value;
}

int param_boolean_int(const char *, int default_value)
{
	return default_value;
}

int param_integer_c(const char *, int default_value, int /*min_val*/, int /*max_val*/ )
{
	return default_value;
}

// stubs for classad_usermap.cpp functions needed by compat_classad
int reconfig_user_maps() { return 0; }
bool user_map_do_mapping(const char *, const char *, MyString & output) { output.clear(); return false; }

END_C_DECLS
int param_integer(const char *, int default_value, int, int, ClassAd *, 
	ClassAd *, bool)
{
	return default_value;
}

int param_integer(const char *, int default_value, int, int, bool)
{
	return default_value;
}

bool param_boolean_crufty(const char *, bool default_value)
{
	return default_value;
}

bool param_boolean( const char *, bool default_value, bool, 
	ClassAd *, ClassAd *, bool)
{
	return default_value;
}


/* stubs for generic query object */
GenericQuery::GenericQuery(void) { 
	integerThreshold = stringThreshold = floatThreshold = 0; 
	integerConstraints = 0; 
	floatConstraints = 0;
	stringConstraints = 0;
	integerKeywordList = floatKeywordList = stringKeywordList = 0;
}
GenericQuery::~GenericQuery(void) {}

/* stubs for query object. */
QueryResult clearStringConstraints  (const int ) 
{ return (QueryResult)not_impl();}

QueryResult clearIntegerConstraints (const int )
{ return (QueryResult)not_impl();}

QueryResult clearFloatConstraints   (const int )
{ return (QueryResult)not_impl();}

void		clearORCustomConstraints(void) 
{ not_impl(); }

void		clearANDCustomConstraints(void) 
{ not_impl(); }

QueryResult CondorQuery::addConstraint (const int , const char *) 
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addConstraint (const int , const int )
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addConstraint (const int, const float)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addORConstraint (const char *)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::addANDConstraint (const char *)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::fetchAds (ClassAdList &, const char [], CondorError* )
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::filterAds (ClassAdList &, ClassAdList &)
{ return (QueryResult)not_impl();}

QueryResult CondorQuery::getQueryAd (ClassAd &)
{ return (QueryResult)not_impl();}

// overloaded operators

// display
/*friend ostream &CondorQuery::operator<< (ostream &foo, CondorQuery &bar) */
/*	{ return (QueryResult)not_impl();}*/

// assignment
/*CondorQuery	&CondorQuery::operator=  (CondorQuery &)*/
/*	{ return (CondorQuery)not_impl();}*/

CondorQuery::CondorQuery(AdTypes ) { not_impl();} 

CondorQuery::~CondorQuery() {} 

#include "Regex.h"

Regex::Regex() {not_impl();}
Regex::~Regex() {}
bool Regex::compile(const char *, char const** , int* , int ) {not_impl();return false;}
bool Regex::compile(MyString const& , char const** , int* , int ) {not_impl();return false;}
bool Regex::match(MyString const& , ExtArray<MyString>* ) {not_impl();return false;}

// CCB me harder
BEGIN_C_DECLS
void Generic_set_log_va(void(*app_log_va)(int level,char*fmt,va_list args))
{
	(void) app_log_va;
};
END_C_DECLS

// Condor Threads stubs - called in dprintf
BEGIN_C_DECLS
int
CondorThreads_pool_size()
{
	return 0;
}

int
CondorThreads_gettid(void)
{
	return -1;
}

#if defined(HAVE_PTHREAD_SIGMASK) && defined(sigprocmask)
	/* Deal with the fact that dprintf.o may be calling pthread_sigmask,
	 * and yet we don't want to require anybody using libcondorapi.a to 
	 * have to drag in all of pthreads. 
	 */
#undef sigprocmask
int pthread_sigmask(int how, const sigset_t *newmask, sigset_t *oldmask)
{
	return sigprocmask(how,newmask,oldmask);
}
#endif

int my_spawnl( const char*, ... )
{ return not_impl(); }

END_C_DECLS

void statusString( int, std::string & )
{ not_impl(); }
