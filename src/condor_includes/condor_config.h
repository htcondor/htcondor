/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONFIG_H
#define CONFIG_H

#if defined(__cplusplus)
#include "condor_classad.h"
#endif

typedef struct bucket {
	char	*name;
	char	*value;
	struct bucket	*next;
} BUCKET;

/*
**  Types of macro expansion
*/
#define EXPAND_LAZY         1
#define EXPAND_IMMEDIATE    2
#define TABLESIZE 113

#if defined(__cplusplus)
extern "C" {
	void config( int wantsQuiet=0 );
	void config_host( char* host=NULL );
	void config_fill_ad( ClassAd* );
	int  set_persistent_config(char *admin, char *config);
	int  set_runtime_config(char *admin, char *config);
	char * expand_macro ( const char *value, BUCKET *table[], int table_size,
						  char *self=NULL );
	int get_var( register char *value, register char **leftp,
	      register char **namep, register char **rightp, char *self=NULL,
		  bool getdollardollar=false);
	int get_env( register char *value, register char **leftp,
				 register char **namep, register char **rightp);
	void lower_case( char *str );
#endif

#if defined(__STDC__) || defined(__cplusplus)
	char * get_tilde();
	char * param ( const char *name );
	int param_integer( const char *name, int default_value );
	void insert ( const char *name, const char *value, BUCKET *table[], int table_size );
	char * lookup_macro ( const char *name, BUCKET *table[], int table_size );
	char * macro_expand ( const char *name );
	int param_in_pattern ( char *parameter, char *pattern);
	void init_config ( void );
	void clear_config ( void );
	void set_debug_flags( char * );
	void config_insert( char*, char* );
#else
	void config();
	void config_host();
	char * get_tilde();
	char * param ();
	insert();
	char * expand_macro();
	char * lookup_macro();
	char * macro_expand();
	int param_in_pattern ();
	void init_config ();
	void clear_config ();
	void config_fill_ad ();
	void set_debug_flags ();
	void config_insert ();
#endif

#if defined(__cplusplus)
}
#endif

#endif /* CONFIG_H */

