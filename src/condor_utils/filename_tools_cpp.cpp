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
#include "condor_debug.h"
#include "condor_config.h"
#include "filename_tools.h"

// keep this function in sync with filename_split() in filename_tools.c
int filename_split( const char *path, std::string &dir, std::string &file ) {
	char const *last_slash;

	last_slash = strrchr(path,DIR_DELIM_CHAR);
	if(last_slash) {
		dir.append(path,last_slash-path);
		last_slash++;
		file = last_slash;
		return 1;
	} else {
		file = path;
		dir = ".";
		return 0;
	}
}

/* Copy in to out, removing whitespace (except spaces). */

// keep in sync with version in filename_tools.c
static void eat_space( const char *in, char *out )
{
	while(1) {
		switch(*in) {
			case 0:
				*out=0;
				return;
				break;
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
Whitespace is trimmed from the string.
*/

// keep in sync with version in filename_tools.c
static char * copy_upto( char *in, char *out, char delim, int length )
{
	int copied=0;
	int escape=0;
	int isleadingws=1;
	char *trimmedend = out;

	while(1) {
		if( *in==0 ) {
			*trimmedend = 0;
			return 0;
		} else if( *in=='\\' && !escape && (in[1] == delim)) {
			escape=1;
			in++;
		} else if( *in==delim && !escape ) {
			*trimmedend = 0;
			return in;
		} else if( isspace(*in) && isleadingws) {
			in++;
		} else {
			isleadingws = 0;
			escape=0;
			if(copied<length) {
				*out++ = *in++;
				copied++;
				if (!isspace(*(in-1))) {
					trimmedend = out;
				}
			} else {
				in++;
			}
		}
	}
}

int filename_remap_find( const char *input, const char *filename, std::string &output, int cur_remap_level )
{
	if (cur_remap_level == 0) {
		dprintf( D_FULLDEBUG, "REMAP: begin with rules: %s\n", input);
	}
	dprintf( D_FULLDEBUG, "REMAP: %i: %s\n", cur_remap_level, filename );

	// if remaps create a loop, this will break it
	int max_remap_level = param_integer("MAX_REMAP_RECURSIONS", 128);
	if (cur_remap_level > max_remap_level) {
		dprintf( D_FULLDEBUG, "REMAP: aborting after %i iterations\n", cur_remap_level);
		output = "<abort>";
		return -1;
	}

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
		char *start = p;
		p = copy_upto(p,name,'=',input_len);
		if(!p) break;

		char *semi = strchr(name, ';');
		if (semi != nullptr) {
			// This could happen if a field is missing an equals, e.g.
			// name1=usr1;missing_equal;name2=usrl1
			p = start + (semi - name) + 1; // skip this field and restart
			continue;
		}

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

	// allow directories to be remapped.
	//
	// above, we attempted to find an exact match of a rule to map <filename>
	// to <output>.
	//
	// if that succeeded, recursively call ourselves again to see if more maps
	// apply.
	//
	// if it did not succeed, then split the filename into parent/entry
	// where entry contains no slashes and recursively call this function in
	// attempt to remap all or some of the parent dir
	//
	// recursion stops when there are no more directories to pop, or we reach a
	// limit (years of careful research says 20) which likely means there is a
	// cycle in the rules.

	if(found) {
		// recurse, using the same remap rules (input)
		std::string new_map;
		int res = filename_remap_find( input, output.c_str(), new_map, cur_remap_level+1 );
		if (res == -1) {
			std::string tmp = output;
			formatstr(output, "<%i: %s>%s", cur_remap_level, filename, new_map.c_str());
			return -1;
		}
		if (res) {
			output = new_map;
		}
	} else {
		std::string parent;
		std::string entry;
    	if(filename_split( filename, parent, entry )) {
			std::string new_parent;
			int res = filename_remap_find( input, parent.c_str(), new_parent, cur_remap_level+1 );
			if (res == -1) {
				formatstr(output, "<%i: %s>%s", cur_remap_level, filename, new_parent.c_str());
				return -1;
			}
			if (res) {
				found = 1;
				formatstr(output, "%s%c%s",new_parent.c_str(),DIR_DELIM_CHAR,entry.c_str());
			}
		} else {
			// can't be split
		}
	}

	return found;
}

// changes all directory separators to match the DIR_DELIM_CHAR
// makes changes in place
void
canonicalize_dir_delimiters( std::string &path ) {

	char *tmp = strdup(path.c_str());
	canonicalize_dir_delimiters( tmp );
	path = tmp;
	free( tmp );
}
