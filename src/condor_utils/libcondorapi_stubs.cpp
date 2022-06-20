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
#include "condor_uid.h"

/* This file contains various stub functions or small implementation of other
	functions. The purpose of this is to break edges in a nasty dependency
	graph between our .o files so we are able to release the user log API
	to the world. 
	-psilord 02/20/03
	*/

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

char* expand_param(const char *str)
{
	if (str) {
		return strdup(str);
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

// stubs for classad_usermap.cpp functions needed by compat_classad
int reconfig_user_maps() { return 0; }
bool user_map_do_mapping(const char *, const char *, MyString & output) { output.clear(); return false; }

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

// stubbing out dprintf, so that users don't end up buffering
// all dprintf calls into memory
typedef unsigned int DebugOutputChoice;
DebugOutputChoice AnyDebugBasicListener;   /* Bits to look for in dprintf */
DebugOutputChoice AnyDebugVerboseListener; /* verbose bits for dprintf */
int _condor_dprintf_works;
void _condor_dprintf_saved_lines( void ) {}
void _condor_save_dprintf_line(int, char const*, ...) {}
void dprintf(int /* level */, const char * /* format */, ...) {}

priv_state _set_priv(priv_state s, const char*, int, int)
{
	static priv_state old_priv = PRIV_UNKNOWN;
	priv_state rc = old_priv;
	old_priv = s;
	return rc;
}

#include "condor_regex.h"

Regex::Regex() {not_impl();}
Regex::~Regex() {}
bool Regex::compile(const char *, int* , int* , uint32_t) {not_impl();return false;}
bool Regex::compile(MyString const& , int*, int* , uint32_t) {not_impl();return false;}
bool Regex::match(MyString const& , ExtArray<MyString>* ) {not_impl();return false;}

// CCB me harder
BEGIN_C_DECLS
void Generic_set_log_va(void(*app_log_va)(int level,char*fmt,va_list args))
{
	(void) app_log_va;
};
END_C_DECLS

int my_spawnl( const char*, ... )
{ return not_impl(); }

void statusString( int, std::string & )
{ not_impl(); }
