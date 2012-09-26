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
#include "condor_classad.h"
#include "condor_string.h"
#include "condor_attributes.h"

#include "env.h"
#include "HashTable.h"
#include "setenv.h"

// Since ';' is the PATH delimiter in Windows, we use a different
// delimiter for V1 environment entries.
static const char unix_env_delim = ';';
static const char windows_env_delim = '|';

#ifndef WIN32
static const char env_delimiter = ';';
#else
static const char env_delimiter = '|';
#endif

Env::Env()
{
	input_was_v1 = false;
	_envTable = new HashTable<MyString, MyString>
		( 127, &MyStringHash, updateDuplicateKeys );
	ASSERT( _envTable );
}

Env::~Env()
{
	delete _envTable;
}

int
Env::Count() const
{
	return _envTable->getNumElements();
}

void
Env::Clear()
{
	_envTable->clear();
#if defined(WIN32)
	m_sorted_varnames.clear();
#endif
}

void
Env::AddErrorMessage(char const *msg,MyString *error_buffer)
{
	ArgList::AddErrorMessage(msg,error_buffer);
}

bool
Env::MergeFrom( const ClassAd *ad, MyString *error_msg )
{
	if(!ad) return true;
	char *env1 = NULL;
	char *env2 = NULL;
	bool merge_success = false;

	if( ad->LookupString(ATTR_JOB_ENVIRONMENT2, &env2) == 1 ) {
		merge_success = MergeFromV2Raw(env2,error_msg);
	}
	else if( ad->LookupString(ATTR_JOB_ENVIRONMENT1, &env1) == 1 ) {
		merge_success = MergeFromV1Raw(env1,error_msg);
		input_was_v1 = true;
	}
	else {
			// this shouldn't be considered an error... maybe the job
			// just doesn't define an environment.  condor_submit always 
			// adds something, but we should't rely on that.
		merge_success = true;
			// leave input_was_v1 untouched... (at dan's recommendation)
	}

	free(env1);
	free(env2);
	return merge_success;
}

bool
Env::CondorVersionRequiresV1(CondorVersionInfo const &condor_version)
{
		// Is it earlier than 6.7.15?
	return !condor_version.built_since_version(6,7,15);
}

bool
Env::InsertEnvIntoClassAd( ClassAd *ad, MyString *error_msg, char const *opsys, CondorVersionInfo *condor_version) const
{

	bool has_env1 = ad->LookupExpr(ATTR_JOB_ENVIRONMENT1) != NULL;
	bool has_env2 = ad->LookupExpr(ATTR_JOB_ENVIRONMENT2) != NULL;

	bool requires_env1 = false;
	if(condor_version) {
		requires_env1 = CondorVersionRequiresV1(*condor_version);
	}

	if(requires_env1) {
		if(has_env2) {
			ad->Delete(ATTR_JOB_ENVIRONMENT2);
		}
	}

	if( (has_env2 || !has_env1) && !requires_env1)
	{
		MyString env2;
		if(!getDelimitedStringV2Raw(&env2,error_msg)) {
			return false;
		}
		ad->Assign(ATTR_JOB_ENVIRONMENT2,env2.Value());
	}
	if(has_env1 || requires_env1) {
		// Record the OPSYS that is being used to delimit the environment.
		char *lookup_delim = NULL;
		char delim = '\0';
		if(opsys) {
			// Use delimiter for target opsys.
			delim = GetEnvV1Delimiter(opsys);
		}
		else if(ad->LookupString(ATTR_JOB_ENVIRONMENT1_DELIM,&lookup_delim)) {
			// Use delimiter that was used previously in this ad.
			delim = *lookup_delim;
		}
		else {
			// Use delimiter for the opsys we are currently running under.
			delim = env_delimiter;
		}

		if(!lookup_delim) {
			// Save the delimiter that we have chosen, in case the ad
			// is read by somebody on a platform that is not the same
			// as opsys.  Example: we are writing the expanded ad in
			// the schedd for a starter on a different opsys, but we
			// want shadows to be able to still parse the environment.

			char delim_str[2];
			delim_str[0] = delim;
			delim_str[1] = '\0';
			ad->Assign(ATTR_JOB_ENVIRONMENT1_DELIM,delim_str);
		}

		MyString env1;
		bool env1_success = getDelimitedStringV1Raw(&env1,error_msg,delim);

		if(lookup_delim) {
			free(lookup_delim);
			lookup_delim = NULL;
		}

		if(env1_success) {
			ad->Assign(ATTR_JOB_ENVIRONMENT1,env1.Value());
		}
		else {
			if(has_env2) {
				// We failed to convert to V1 syntax, but we started
				// with V2, so this is a special kind of failure.
				// Rather than failing outright, simply stick
				// an invalid environment value in the V1 attribute.
				// This happens, for example, when the schedd is
				// generating the expanded ad to send to an older
				// starter that does not understand V2 syntax.
				// The end result in this case is to fail when the
				// starter tries to read the environment, and then
				// we go back and get a new match, which hopefully
				// is a machine that understands V2 syntax.  A more
				// direct mechanism would be nice, but this one should
				// at least prevent incorrect behavior.

				ad->Assign(ATTR_JOB_ENVIRONMENT1,"ENVIRONMENT_CONVERSION_ERROR");
				dprintf(D_FULLDEBUG,"Failed to convert environment to V1 syntax: %s\n",error_msg ? error_msg->Value() : "");
			}
			else {
				// Failed to convert to V1 syntax, and the ad does not
				// already contain V2 syntax, so we should assume the
				// worst.
				AddErrorMessage("Failed to convert to target environment syntax.",error_msg);
				return false;
			}
		}
	}
	return true;
}

bool
Env::MergeFrom( char const * const *stringArray )
{
	if( !stringArray ) {
		return false;
	}
	int i;
	bool all_ok = true;
	for( i = 0; stringArray[i] && stringArray[i][0] != '\0'; i++ ) {
		if( !SetEnv( stringArray[i] ) ) {
				// Keep going so that we behave like getenv() in
				// our treatment of invalid entries in the
				// environment.  However, this function still
				// returns error, unlike Import().
			all_ok = false;
		}
	}
	return all_ok;
}

bool
Env::MergeFrom( char const * env_str )
{
	if ( !env_str ) {
		return false;
	}
	const char *ptr = env_str;
	while ( *ptr != '\0' ) {
		// a Windows environment block typically contains stuff like
		// '=::=::\' and '=C:=C:\some\path'; SetEnv will return an error
		// for strings like these, so we'll just silently ignore errors
		// from SetEnv and insert what we can
		//
		SetEnv( ptr );
		ptr += strlen(ptr) + 1;
	}
	return true;
}

void
Env::MergeFrom( Env const &env )
{
	MyString var,val;

	env._envTable->startIterations();
	while(env._envTable->iterate(var,val)) {
		ASSERT(SetEnv(var,val));
	}
}

bool
Env::IsSafeEnvV1Value(char const *str,char delim)
{
	// This is used to detect whether the given environment value
	// is unexpressable in the old environment syntax (before environment2).

	if(!str) return false;
	if(!delim) delim = env_delimiter;

	char specials[] = {'|','\n','\0'};
	// Some compilers do not like env_delimiter to be in the
	// initialization constant.
	specials[0] = delim;

	size_t safe_length = strcspn(str,specials);

	// If safe_length goes to the end of the string, we are okay.
	return !str[safe_length];
}

bool
Env::IsSafeEnvV2Value(char const *str)
{
	// This is used to detect whether the given environment value
	// is unexpressable in the environment2 syntax.

	if(!str) return false;

	// Newline is an unsafe character, because the schedd (as of
	// 6.8.3) cannot handle newlines in ClassAd attributes.

	char specials[] = {'\n','\0'};

	size_t safe_length = strcspn(str,specials);

	// If safe_length goes to the end of the string, we are okay.
	return !str[safe_length];
}

void
Env::WriteToDelimitedString(char const *input,MyString &output) {
	// Append input to output.
	// Would be nice to escape special characters here, but the
	// existing syntax does not support it, so we leave the
	// "specials" strings blank.

	char const inner_specials[] = {'\0'};
	char const first_specials[] = {'\0'};

	char const *specials = first_specials;
	char const *end;
	bool ret;

	if(!input) return;

	while(*input) {
		end = input + strcspn(input,specials);
		ret = output.formatstr_cat("%.*s", (int)(end-input), input);
		ASSERT(ret);
		input = end;

		if(*input != '\0') {
			// Escape this special character.
			// Escaping is not yet implemented, so we will never get here.
			ret = output.formatstr_cat("%c",*input);
			ASSERT(ret);
			input++;
		}

		// Switch out of first-character escaping mode.
		specials = inner_specials;
	}
}

bool
Env::ReadFromDelimitedString(char const *&input, char *output) {
	// output buffer must be big enough to hold next environment entry
	// (to be safe, it should be same size as input buffer)

	// strip leading (non-escaped) whitespace
	while( *input==' ' || *input=='\t' || *input=='\n'  || *input=='\r') {
		input++;
	}

	while( *input ) {
		if(*input == '\n' || *input == env_delimiter) {
			// for backwards compatibility with old env parsing in environ.C,
			// we also treat '\n' as a valid delimiter
			input++;
			break;
		}
		else {
			// all other characters are copied verbatim
			*(output++) = *(input++);
		}
	}

	*output = '\0';

	return true;
}

bool
Env::IsV2QuotedString(char const *str)
{
	return ArgList::IsV2QuotedString(str);
}

bool
Env::V2QuotedToV2Raw(char const *v1_quoted,MyString *v2_raw,MyString *errmsg)
{
	return ArgList::V2QuotedToV2Raw(v1_quoted,v2_raw,errmsg);
}

bool
Env::MergeFromV1RawOrV2Quoted( const char *delimitedString, MyString *error_msg )
{
	if(!delimitedString) return true;
	if(IsV2QuotedString(delimitedString)) {
		MyString v2;
		if(!V2QuotedToV2Raw(delimitedString,&v2,error_msg)) {
			return false;
		}
		return MergeFromV2Raw(v2.Value(),error_msg);
	}
	else {
		return MergeFromV1Raw(delimitedString,error_msg);
	}
}

bool
Env::MergeFromV2Quoted( const char *delimitedString, MyString *error_msg )
{
	if(!delimitedString) return true;
	if(IsV2QuotedString(delimitedString)) {
		MyString v2;
		if(!V2QuotedToV2Raw(delimitedString,&v2,error_msg)) {
			return false;
		}
		return MergeFromV2Raw(v2.Value(),error_msg);
	}
	else {
		AddErrorMessage("Expecting a double-quoted environment string (V2 format).",error_msg);
		return false;
	}
}

bool
Env::MergeFromV2Raw( const char *delimitedString, MyString *error_msg )
{
	SimpleList<MyString> env_list;

	if(!delimitedString) return true;

	if(!split_args(delimitedString,&env_list,error_msg)) {
		return false;
	}

	SimpleListIterator<MyString> it(env_list);
	MyString *env_entry;
	while(it.Next(env_entry)) {
		if(!SetEnvWithErrorMessage(env_entry->Value(),error_msg)) {
			return false;
		}
	}
	return true;
}

bool
Env::MergeFromV1Raw( const char *delimitedString, MyString *error_msg )
{
	char const *input;
	char *output;
	int outputlen;
	bool retval = true;

	input_was_v1 = true;
	if(!delimitedString) return true;

	// create a buffer big enough to hold any of the individual env expressions
	outputlen = strlen(delimitedString)+1;
	output = new char[outputlen];
	ASSERT(output);

	input = delimitedString;
	while( *input ) {
		retval = ReadFromDelimitedString(input,output);

		if(!retval) {
			break; //failed to parse environment string
		}

		if(*output) {
			retval = SetEnvWithErrorMessage(output,error_msg);
			if(!retval) {
				break; //failed to add environment expression
			}
		}
	}
	delete[] output;
	return retval;
}

// It is not possible for raw V1 environment strings with a leading space
// to be specified in submit files, so we can use this to mark
// V2 strings when we need to pack V1 and V2 through the same
// channel (e.g. shadow-starter communication).
const char RAW_V2_ENV_MARKER = ' ';

bool
Env::MergeFromV1or2Raw( const char *delimitedString, MyString *error_msg )
{
	if(!delimitedString) return true;
	if(*delimitedString == RAW_V2_ENV_MARKER) {
		return MergeFromV2Raw(delimitedString,error_msg);
	}
	else {
		return MergeFromV1Raw(delimitedString,error_msg);
	}
}

// The following is a modest hack for when we find
// a $$() expression in the environment and we just want to
// pass it through verbatim, with no appended equal sign.
char const *NO_ENVIRONMENT_VALUE = "\01\02\03\04\05\06";

bool
Env::SetEnvWithErrorMessage( const char *nameValueExpr, MyString *error_msg )
{
	char *expr, *delim;
	int retval;

	if( nameValueExpr == NULL || nameValueExpr[0] == '\0' ) {
		return false;
	}

	// make a copy of nameValueExpr for modifying
	expr = strnewp( nameValueExpr );
	ASSERT( expr );

	// find the delimiter
	delim = strchr( expr, '=' );

	if(delim == NULL && strstr(expr,"$$")) {
		// This environment entry is an unexpanded $$() macro.
		// We just want to keep it in the environment verbatim.
		SetEnv(expr,NO_ENVIRONMENT_VALUE);
		delete[] expr;
		return true;
	}

	// fail if either name or delim is missing
	if( expr == delim || delim == NULL ) {
		if(error_msg) {
			MyString msg;
			if(delim == NULL) {
				msg.formatstr(
				  "ERROR: Missing '=' after environment variable '%s'.",
				  nameValueExpr);
			}
			else {
				msg.formatstr("ERROR: missing variable in '%s'.",expr);
			}
			AddErrorMessage(msg.Value(),error_msg);
		}
		delete[] expr;
		return false;
	}

	// overwrite delim with '\0' so we have two valid strings
	*delim = '\0';

	// do the deed
	retval = SetEnv( expr, delim + 1 );
	delete[] expr;
	return retval;
}

bool
Env::SetEnv( const char* var, const char* val )
{
	MyString myVar = var;
	MyString myVal = val;
	return SetEnv( myVar, myVal );
}

bool
Env::SetEnv( const MyString & var, const MyString & val )
{
	if( var.Length() == 0 ) {
		return false;
	}
	bool ret = (_envTable->insert( var, val ) == 0);
	ASSERT( ret );
#if defined(WIN32)
	m_sorted_varnames.erase(var.Value());
	m_sorted_varnames.insert(var.Value());
#endif
	return true;
}

bool
Env::getDelimitedStringV1or2Raw(ClassAd const *ad,MyString *result,MyString *error_msg)
{
	Clear();
	if(!MergeFrom(ad,error_msg)) {
		return false;
	}

	char *lookup_delim = NULL;
	char delim = env_delimiter;
	ad->LookupString(ATTR_JOB_ENVIRONMENT1_DELIM,&lookup_delim);
	if(lookup_delim) {
		delim = *lookup_delim;
		free(lookup_delim);
	}

	return getDelimitedStringV1or2Raw(result,error_msg,delim);
}

bool
Env::getDelimitedStringV1or2Raw(MyString *result,MyString *error_msg,char v1_delim) const
{
	ASSERT(result);
	int old_len = result->Length();

	if(getDelimitedStringV1Raw(result,NULL,v1_delim)) {
		return true;
	}

	// V1 attempt failed.  Use V2 syntax.

	if(result->Length() > old_len) {
		// Clear any partial output we may have generated above.
		result->setChar(old_len,'\0');
	}

	return getDelimitedStringV2Raw(result,error_msg,true);
}

bool
Env::getDelimitedStringV2Quoted(MyString *result,MyString *error_msg) const
{
	MyString v2_raw;
	if(!getDelimitedStringV2Raw(&v2_raw,error_msg)) {
		return false;
	}
	ArgList::V2RawToV2Quoted(v2_raw,result);
	return true;
}

bool
Env::getDelimitedStringV1RawOrV2Quoted(MyString *result,MyString *error_msg) const
{
	if(getDelimitedStringV1Raw(result,NULL)) {
		return true;
	}
	else {
		result->setChar(0, '\0');
		return getDelimitedStringV2Quoted(result,error_msg);
	}
}

bool
Env::getDelimitedStringV2Raw(MyString *result,MyString * /*error_msg*/,bool mark_v2) const
{
	MyString var, val;
	SimpleList<MyString> env_list;

	ASSERT(result);

	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		if(val == NO_ENVIRONMENT_VALUE) {
			env_list.Append(var);
		}
		else {
			MyString var_val;
			var_val.formatstr("%s=%s",var.Value(),val.Value());
			env_list.Append(var_val);
		}
	}

	if(mark_v2) {
		(*result) += RAW_V2_ENV_MARKER;
	}
	join_args(env_list,result);
	return true;
}

void
Env::getDelimitedStringForDisplay(MyString *result) const
{
	ASSERT(result);
	getDelimitedStringV2Raw(result,NULL);
}

char
Env::GetEnvV1Delimiter(char const *opsys)
{
	if(!opsys) {
		return env_delimiter;
	}
	else if(!strncmp(opsys,"WIN",3)) { // match "WINDOWS" or "WINNTnn" or "WIN32"
		return windows_env_delim;
	}
	else {
		return unix_env_delim;
	}
}

bool
Env::getDelimitedStringV1Raw(MyString *result,MyString *error_msg,char delim) const
{
	MyString var, val;

	bool emptyString = true;
	if(!delim) delim = env_delimiter;

	ASSERT(result);

	_envTable->startIterations();
	while( _envTable->iterate( var, val ) ) {
		if(!IsSafeEnvV1Value(var.Value(),delim) ||
		   !IsSafeEnvV1Value(val.Value(),delim)) {

			if(error_msg) {
				MyString msg;
				msg.formatstr("Environment entry is not compatible with V1 syntax: %s=%s",var.Value(),val.Value());
				AddErrorMessage(msg.Value(),error_msg);
			}
			return false;
		}
		// only insert the delimiter if there's already an entry...
        if( !emptyString ) {
			(*result) += delim;
        }
		WriteToDelimitedString(var.Value(),*result);
		if(val != NO_ENVIRONMENT_VALUE) {
			WriteToDelimitedString("=",*result);
			WriteToDelimitedString(val.Value(),*result);
		}
		emptyString = false;
	}
	return true;
}

#if defined(WIN32)
char*
Env::getWindowsEnvironmentString() const
{
	// this returns an environment string in the format needed
	// for passing to the Windows CreateProcess function. this
	// means it is null-delimited, null-terminated, and sorted
	// (igoring case - all variable names must be converted to
	// uppercase before comparing)

	std::string output;
	typedef std::set<std::string, toupper_string_less> set_t;
	for (set_t::const_iterator i = m_sorted_varnames.begin();
	     i != m_sorted_varnames.end();
	     i++)
	{
		MyString val;
		int ret = _envTable->lookup(i->c_str(), val);
		ASSERT(ret != -1);
		output += *i;
		output += '=';
		output += val.Value();
		output += '\0';
	}
	output += '\0';
	char* ret = new char[output.size()];
	ASSERT(ret != NULL);
	output.copy(ret, output.size());
	return ret;
}
#endif

char **
Env::getStringArray() const {
	char **array = NULL;
	int numVars = _envTable->getNumElements();
	int i;

	array = new char*[ numVars+1 ];
	ASSERT( array );

    MyString var, val;

    _envTable->startIterations();
	for( i = 0; _envTable->iterate( var, val ); i++ ) {
		ASSERT( i < numVars );
		ASSERT( var.Length() > 0 );
		array[i] = new char[ var.Length() + val.Length() + 2 ];
		ASSERT( array[i] );
		strcpy( array[i], var.Value() );
		if(val != NO_ENVIRONMENT_VALUE) {
			strcat( array[i], "=" );
			strcat( array[i], val.Value() );
		}
	}
	array[i] = NULL;
	return array;
}

bool
Env::GetEnv(MyString const &var,MyString &val) const
{
	// lookup returns 0 on success
	return _envTable->lookup(var,val) == 0;
}

void
Env::Import( void )
{
	char **my_environ = GetEnviron();
	for (int i=0; my_environ[i]; i++) {
		const char	*p = my_environ[i];

		int			j;
		MyString	varname = "";
		MyString	value = "";
		for (j=0;  ( p[j] != '\0' ) && ( p[j] != '=' );  j++) {
			varname += p[j];
		}
		if ( p[j] == '\0' ) {
				// ignore entries in the environment that do not
				// contain an assignment
			continue;
		}
		if ( varname.IsEmpty() ) {
				// ignore entries in the environment that contain
				// an empty variable name
			continue;
		}
		ASSERT( p[j] == '=' );
		value = p+j+1;

		// Allow the application to filter the import
		if ( ImportFilter( varname, value ) ) {
			bool ret = SetEnv( varname, value );
			ASSERT( ret ); // should never fail
		}
	}
}
