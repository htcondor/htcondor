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
#include "filename_tools.h"

void filename_url_parse( char *input, char *method, char *server, int *port, char *path )
{
	char *p,*q;

	method[0] = server[0] = path[0] = 0;
	*port = -1;

	/* Find a colon and record the method proceeding it */

	p = strchr(input,':');
	if(p) {
		*p=0;
		strncpy(method,input,_POSIX_PATH_MAX);
		*p=':';
		p++;
	} else {
		p = input;
	}

	/* Now p points to the first character following the colon */

	if( (p[0]=='/') && (p[1]=='/') ) {

		/* This is a server name, skip the slashes */
		p++; p++;

		/* Find the end of the server name */
		q = strchr(p,'/');

		if(q) {
			/* If there is an end, copy up to that. */
			*q=0;
			strncpy(server,p,_POSIX_PATH_MAX);
			*q='/';
			p = q++;
		} else {
		        /* If there is no end, copy all of it. */
			strncpy(server,p,_POSIX_PATH_MAX);
			p = 0;
		}

		/* Now, reconsider the server name for a port number. */
		q = strchr(server,':');

		if(q) {
			/* If there is one, delimit the server name and scan */
			*q=0;
			q++;
			*port = atoi(q);
		}
	}

	/* Now, p points to the beginning of the filename, or null */

	if( (!p) || (!*p) ) return;

	strncpy(path,p,_POSIX_PATH_MAX);
}

/* Copy in to out, removing whitespace. */

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


int filename_remap_find( const char *input, const char *filename, char *output )
{
	char name[_POSIX_PATH_MAX];
	char url[_POSIX_PATH_MAX];
	char *buffer,*p;

	/* First make a copy of the input in canonical form */

	buffer = malloc(strlen(input)+1);
	if(!buffer) return 0;
	eat_space(input,buffer);

	/* Now find things like name=url; name=url; ... */
	/* A trailing url with no ; shouldn't cause harm. */

	p = buffer;

	while(1) {
		p = copy_upto(p,name,'=',_POSIX_PATH_MAX);
		if(!p) break;
		p++;
		p = copy_upto(p,url,';',_POSIX_PATH_MAX);

		if(!strncmp(name,filename,_POSIX_PATH_MAX)) {
			strncpy(output,url,_POSIX_PATH_MAX);
			free(buffer);
			return 1;
		}

		if(!p) break;
		p++;
	}

	free(buffer);
	return 0;
}

int is_relative_to_cwd( const char *path )
{
#if WIN32
	if(*path == '/' || *path == '\\') return 0;
	if(('A' <= toupper(*path) && toupper(*path) <= 'Z') && path[1] == ':') return 0;
#else
	if(*path == DIR_DELIM_CHAR) return 0;
#endif
	return 1;
}

int filename_split( const char *path, char *dir, char *file )
{
	char *last_slash;

	last_slash = strrchr(path,DIR_DELIM_CHAR);
	if(last_slash) {
		strncpy(dir,path,(last_slash-path));
		dir[(last_slash-path)] = 0;
       		last_slash++;
		strcpy(file,last_slash);
		return 1;
	} else {
		strcpy(file,path);
		strcpy(dir,".");
		return 0;
	}
}

// changes all directory separators to match the DIR_DELIM_CHAR
// makes changes in place
void
canonicalize_dir_delimiters( char *path ) {

	int loc, len;
	char *path_ptr;
	
	loc = 0;
	path_ptr = path;
	len = strlen(path);

	while ( loc < len ) {
		if ( path[loc] == '\\' || path[loc] == '/' ) {
		   	path[loc] = DIR_DELIM_CHAR;
		}
		loc++;
	}	
}
