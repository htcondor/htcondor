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
#include "filename_tools.h"
#include "MyString.h"

// keep this function in sync with filename_split() in filename_tools.c
int filename_split( const char *path, MyString &dir, MyString &file )
{
	char const *last_slash;

	last_slash = strrchr(path,DIR_DELIM_CHAR);
	if(last_slash) {
		dir = path;
		dir.setChar((last_slash-path),'\0');
		last_slash++;
		file = last_slash;
		return 1;
	} else {
		file = path;
		dir = ".";
		return 0;
	}
}

/* Copy in to out, removing whitespace. */

// keep in sync with version in filename_tools.c
static void eat_space( const char *in, char *out )
{
	while(1) {
		switch(*in) {
			case 0:
				*out=0;
				return;
				break;
			case ' ':
			case '\t':
			case '\n':
				in++;
				break;
			default:
				*out++ = *in++;
				break;
		}
	}
}

/*
Copy from in to out, stopping at null or delim.  If the amount
to copy exceeds length, eat the rest silently.  Return a
pointer to the delimiter, or return null at end of string.
The character can be escaped with a backslash.
*/

// keep in sync with version in filename_tools.c
static char * copy_upto( char *in, char *out, char delim, int length )
{
	int copied=0;
	int escape=0;

	while(1) {
		if( *in==0 ) {
			*out=0;
			return 0;
		} else if( *in=='\\' && !escape ) {
			escape=1;
			in++;
		} else if( *in==delim && !escape ) {
			*out=0;
			return in;
		} else {
			escape=0;
			if(copied<length) {
				*out++ = *in++;
				copied++;
			} else {
				in++;
			}
		}
	}
}

int filename_remap_find( const char *input, const char *filename, MyString &output )
{
	char *name;
	char *url;
	char *buffer,*p;
	int found = 0;
	size_t input_len = strlen(input);

	/* First make a copy of the input in canonical form */

	buffer = (char *)malloc(input_len+1);
	name = (char *)malloc(input_len+1);
	url = (char *)malloc(input_len+1);
	if(!buffer || !name || !url) {
		free(buffer);
		free(name);
		free(url);
		return 0;
	}
	eat_space(input,buffer);

	/* Now find things like name=url; name=url; ... */
	/* A trailing url with no ; shouldn't cause harm. */

	p = buffer;

	while(1) {
		p = copy_upto(p,name,'=',input_len);
		if(!p) break;
		p++;
		p = copy_upto(p,url,';',input_len);

		if(!strncmp(name,filename,input_len)) {
			output = url;
			found = 1;
			break;
		}

		if(!p) break;
		p++;
	}

	free(buffer);
	free(name);
	free(url);
	return found;
}

// changes all directory separators to match the DIR_DELIM_CHAR
// makes changes in place
void
canonicalize_dir_delimiters( MyString &path ) {

	char *tmp = strdup(path.Value());
	canonicalize_dir_delimiters( tmp );
	path = tmp;
	free( tmp );
}

void filename_url_parse( char *input, MyString &method, MyString &server, int *port, MyString &path ) {
	char *tmp_method = NULL;
	char *tmp_server = NULL;
	char *tmp_path = NULL;
	filename_url_parse_malloc(input,&tmp_method,&tmp_server,port,&tmp_path);
	method = tmp_method;
	server = tmp_server;
	path = tmp_path;
	free( tmp_method );
	free( tmp_server );
	free( tmp_path );
}
