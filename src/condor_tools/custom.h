/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#ifndef _MACRO_H
#define _MACRO_H

#include <stdio.h>

const int TRUE = 1;
const int FALSE = 0;

int confirm( const char *prompt );


/*
  Construct a macro by dialoging with the user.  The function gen() will
  then generate the string which defines the macro.
*/
class Macro {
public:
	Macro( const char *name, const char *prompt, const char *dflt );
	void init( int (*f)(const char *) = 0 );
	void init( int truth );
	char *get_val() { return value; }
	char *gen( int shell_cmd = 0 );
private:
	char	*name;
	char	*prompt;
	char	*dflt;
	char	*value;
};

/*
  Construct a customized file from a generic one.  The new file has macros
  added by the add_macro() function, but is otherwise an identical copy
  of the generic version.
*/
class ConfigFile {
public:
	ConfigFile( const char *directory );
		// these routines exit upon error
	void	begin( const char *name );
	void	add_macro( const char *def );
	void	end();
private:
	char	*dir;
	FILE	*src;
	FILE	*dst;
};


#endif
