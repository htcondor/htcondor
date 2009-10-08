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

bool param_boolean( const char *, const bool default_value, bool, 
	ClassAd *, ClassAd *, bool)
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
	int Stream::code(void *&) { return not_impl(); }
	int Stream::code(char &) { return not_impl(); }
	int Stream::code(unsigned char &) { return not_impl(); }
	int Stream::code(int &){ return not_impl(); }
	int Stream::code(unsigned int &){ return not_impl(); }
	int Stream::code(long &){ return not_impl(); }
	int Stream::code(unsigned long &){ return not_impl(); }
#if !defined(__LP64__)
	int Stream::code(int64_t &){ return not_impl(); }
	int Stream::code(uint64_t &){ return not_impl(); }
#endif
	int Stream::code(short &){ return not_impl(); }
	int Stream::code(unsigned short &){ return not_impl(); }
	int Stream::code(float &){ return not_impl(); }
	int Stream::code(double &){ return not_impl(); }
	int Stream::code(char *&){ return not_impl(); }
	int Stream::code(char *&, int &){ return not_impl(); }
	int Stream::code_bytes(void *, int ){ return not_impl(); }
	int Stream::code_bytes_bool(void *, int ){ return not_impl(); }
	int Stream::code(PROC_ID &){ return not_impl(); }
	int Stream::code(STARTUP_INFO &){ return not_impl(); }
	int Stream::code(PORTS &){ return not_impl(); }
	int Stream::code(StartdRec &){ return not_impl(); }
	int Stream::code(open_flags_t &){ return not_impl(); }
	int Stream::code(struct stat &){ return not_impl(); }
	int Stream::code(condor_errno_t &){ return not_impl(); }
#if !defined(WIN32)
	int Stream::code(condor_signal_t &){ return not_impl(); }
	int Stream::code(fcntl_cmd_t &){ return not_impl(); }
	int Stream::code(struct rusage &){ return not_impl(); }
	int Stream::code(struct statfs &){ return not_impl(); }
	int Stream::code(struct timezone &){ return not_impl(); }
	int Stream::code(struct timeval &){ return not_impl(); }
	int Stream::code(struct utimbuf &){ return not_impl(); }
	int Stream::code(struct rlimit &){ return not_impl(); }
	int Stream::code_array(gid_t *&, int &){ return not_impl(); }
	int Stream::code(struct utsname &){ return not_impl(); }
#endif // !defined(WIN32)
#if HAS_64BIT_STRUCTS
	int Stream::code(struct stat64 &){ return not_impl(); }
	int Stream::code(struct rlimit64 &){ return not_impl(); }
#endif
void Stream::allow_one_empty_message() { not_impl(); }
int Stream::put(char const *){ return not_impl(); }
int Stream::get(char *&){ return not_impl(); }
int Stream::get(char *,int ){ return not_impl(); }
int Stream::get_string_ptr(char const *&){ return not_impl(); }

void Stream::prepare_crypto_for_secret(){not_impl();}
void Stream::restore_crypto_after_secret(){not_impl();}
bool Stream::prepare_crypto_for_secret_is_noop(){not_impl();return true;}
void Stream::set_crypto_mode(bool enabled){not_impl();}
bool Stream::get_encryption() const{not_impl();return false;}
int Stream::put_secret( char const *s ){not_impl();return 0;}
int Stream::get_secret( char *&s ){not_impl();return 0;}
void Stream::set_deadline_timeout(int){not_impl();}
void Stream::set_deadline(time_t){not_impl();}
time_t Stream::get_deadline(){not_impl();return 0;}
bool Stream::deadline_expired(){not_impl();return false;}


/* stubs for generic query object */
GenericQuery::GenericQuery(void) {}
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

char*
my_ip_string() {not_impl(); return 0;}

void ConvertDefaultIPToSocketIP(char const *,char const *,char **,Stream& ) {
	not_impl();
}

void ConvertDefaultIPToSocketIP(char const *,char **,Stream& ) {
	not_impl();
}

#include "Regex.h"

Regex::Regex() {not_impl();}
Regex::~Regex() {not_impl();}
bool Regex::compile(MyString const& , char const** , int* , int ) {not_impl();return false;}
bool Regex::match(MyString const& , ExtArray<MyString>* ) {not_impl();return false;}

#ifndef WIN32
bool privsep_enabled() { return false; }
int privsep_open(uid_t, gid_t, const char*, int, mode_t) { not_impl(); return 0;}
#endif

// GCB me harder
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

END_C_DECLS
