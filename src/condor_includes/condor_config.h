#ifndef CONFIG_H
#define CONFIG_H

#include "condor_expressions.h"

typedef struct bucket {
	char	*name;
	char	*value;
	struct bucket	*next;
} BUCKET;


#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__STDC__) || defined(__cplusplus)
void config ( char *a_out_name, CONTEXT *context );
char * param ( char *name );
void insert ( char *name, char *value, BUCKET *table[], int table_size );
char * expand_macro ( char *value, BUCKET *table[], int table_size );
char * lookup_macro ( char *name, BUCKET *table[], int table_size );
#else
void config ();
char * param ();
insert();
char * expand_macro();
char * lookup_macro();
#endif

#if defined(__cplusplus)
}
#endif

#endif
