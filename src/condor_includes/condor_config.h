#ifndef CONFIG_H
#define CONFIG_H

#include "condor_expressions.h"
#include "condor_classad.h"

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
#endif

#if defined(__STDC__) || defined(__cplusplus)
	void config( ClassAd* classAd );
	void config_master( ClassAd* classAd );
	int	 config_from_server(char*, char*, CONTEXT*);
	char * param ( char *name );
	void insert ( char *name, char *value, BUCKET *table[], int table_size );
	char * expand_macro ( char *value, BUCKET *table[], int table_size );
	char * lookup_macro ( char *name, BUCKET *table[], int table_size );
	char * macro_expand ( char *name );
	int param_in_pattern ( char *parameter, char *pattern);
	void init_config ( void );
	void set_debug_flags( char * );
#else
	void config();
	void config_master();
	int	 config_from_server();
	char * param ();
	insert();
	char * expand_macro();
	char * lookup_macro();
	char * macro_expand();
	int param_in_pattern ();
	void init_config ();
	void set_debug_flags ();
#endif

#if defined(__cplusplus)
}
#endif

#endif
