/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Anand Narayanan
** 
*/ 

/*
** This file implements configAd(), a classad version of config().
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include "trace.h"
#include "files.h"
#include "debug.h"
#include "except.h"
#include "clib.h"
#include "condor_sys.h"
#include "condor_config.h"
#include "condor_classad.h"

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef LINT
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
#endif LINT

int SetSyscalls(int);

extern BUCKET	*ConfigTab[];

char	*expand_macro(), *lookup_macro(), *param();

char *get_arch();
char *get_op_sys();

extern int	ConfigLineNo;

#if defined(__STDC__)
void insert_context( char *name, char *value, CONTEXT *context );
#else
void insert_context();
#endif

#if !defined(USER_NAME)
#	define USER_NAME "condor"
#endif

/* conversion to ClassAd -- N Anand*/
void configAd(char* a_out_name, ClassAd* classAd)
{
  struct passwd	*pw, *getpwnam();
  char			*ptr;
  char			*config_file, *tilde;
  int				testing, rval;
  char			hostname[1024];
  int				scm;
  char			*arch, *op_sys;
  
  /*
   ** N.B. if we are using the yellow pages, system calls which are
   ** not supported by either remote system calls or file descriptor
   ** mapping will occur.  Thus we must be in LOCAL/UNRECORDED mode here.
   */
  scm = SetSyscalls( SYS_LOCAL | SYS_UNRECORDED );
  if( (pw=getpwnam(USER_NAME)) == NULL ) {
	printf( "Can't find user \"%s\" in passwd file!\n", USER_NAME );
	exit( 1 );
  }
  (void)endpwent();
  (void)SetSyscalls( scm );
  
  // build a hash table with entries for "tilde" and
  // "hostname". Note that "hostname" ends up as just the
  // machine name w/o the . separators
  tilde = strdup( pw->pw_dir );
  insert( "tilde", tilde, ConfigTab, TABLESIZE );
  free( tilde );
  
  if( gethostname(hostname,sizeof(hostname)) < 0 ) 
  {
	fprintf( stderr, "gethostname failed, errno = %d\n", errno );
	exit( 1 );
  }
  
  if( ptr=(char *)strchr((const char *)hostname,'.') )
	*ptr = '\0';
  insert( "hostname", hostname, ConfigTab, TABLESIZE );

  
  /* Different cases for default configuration or server configuration */
  /* weiru */
  /* Test versions end in _t, prog name gets passed in */
  for( ptr=a_out_name; *ptr; ptr++ );
  if( strcmp("_t",ptr-2) == 0 ) 
  {
	testing = 1;
  }
  else 
  {
	testing = 0;
  }
  
  // In h/files.h:
  // #define CONFIG                          "condor_config"
  // #define CONFIG_TEST                     "condor_config_t"
  
  config_file = (testing)?(CONFIG_TEST):(CONFIG);

  // read the configuration file and produce a classAd
  // also build the hash table to store values
  rval = Read_config( pw->pw_dir, config_file, classAd,
		     ConfigTab, TABLESIZE, EXPAND_LAZY );
  
  if( rval < 0 ) 
  {
	fprintf( stderr,
			"Configuration Error Line %d while processing config file %s/%s ",
			ConfigLineNo, pw->pw_dir, config_file );
	perror( "" );
	exit( 1 );
  }
  
  if( (config_file=param("LOCAL_CONFIG_FILE")) ) 
  {
	pw->pw_dir[0] = '\0';	/* Name specified in global config file */
  } 
  else 
  {
	if( testing ) 
	{		
	  // Default to: "~condor/condor_config_t.local" 
	  config_file = strdup( LOCAL_CONFIG_TEST );
	} 
	else 
	{
	  // Default to: "~condor/condor_config.local" 
	  config_file = strdup( LOCAL_CONFIG );
	}
  }
  
  (void) Read_config( pw->pw_dir, config_file, classAd,
					 ConfigTab, TABLESIZE, EXPAND_LAZY );
  free( config_file );

  // ToDo -> move this to read_config(..) -> NA


  /* If ARCH is not defined in config file, then try to get value
     using uname().  Note that the config file parameter overrides
     the uname() value.  -Jim B. */
  /* Jim's insertion into context changed to insertion into
     classAd -> N Anand */
  
  if( (param("ARCH") == NULL) && ((arch = get_arch()) != NULL) ) 
  {
    insert( "ARCH", arch, ConfigTab, TABLESIZE );
    if(classAd)
    {
      char line[80];
      sprintf(line,"%s = %s","ARCH",arch);
      classAd->Insert(line);
    }
  }

  /* If OPSYS is not defined in config file, then try to get value
     using uname().  Note that the config file parameter overrides
     the uname() value.  -Jim B. */
  /* Jim's insertion into context changed to insertion into
     classAd -> N Anand */
  if( (param("OPSYS") == NULL) && ((op_sys = get_op_sys()) != NULL) ) 
  {
    insert( "OPSYS", op_sys, ConfigTab, TABLESIZE );
    if(classAd)
    {
      char line[80];
      sprintf(line,"%s = %s","OPSYS",op_sys);
      classAd->Insert(line);
    }
  }
}

#if defined(__cplusplus)
}
#endif
